#include <boutexception.hxx>
#include <field_factory.hxx> // Used for parsing expressions
#include <options.hxx>
#include <output.hxx>
#include <utils.hxx>

#include <iomanip>
#include <sstream>

/// The source label given to default values
const std::string Options::DEFAULT_SOURCE{_("default")};
Options *Options::root_instance{nullptr};

Options &Options::root() {
  if (root_instance == nullptr) {
    // Create the singleton
    root_instance = new Options();
  }
  return *root_instance;
}

void Options::cleanup() {
  if (root_instance == nullptr)
    return;
  delete root_instance;
  root_instance = nullptr;
}

Options::Options(const Options& other)
    : value(other.value), attributes(other.attributes),
      parent_instance(other.parent_instance), full_name(other.full_name),
      is_section(other.is_section), children(other.children), is_value(other.is_value),
      value_used(other.value_used) {

  // Ensure that this is the parent of all children,
  // otherwise will point to the original Options instance
  for (auto& child : children) {
    child.second.parent_instance = this;
  }
}

Options &Options::operator[](const std::string &name) {
  // Mark this object as being a section
  is_section = true;

  if (name.empty()) {
    return *this;
  }

  // Find and return if already exists
  auto it = children.find(lowercase(name));
  if (it != children.end()) {
    return it->second;
  }

  // Doesn't exist yet, so add
  std::string secname = name;
  if (!full_name.empty()) { // prepend the section name
    secname = full_name + ":" + secname;
  }

  // emplace returns a pair with iterator first, boolean (insert yes/no) second
  auto pair_it = children.emplace(lowercase(name), Options{this, secname});

  return pair_it.first->second;
}

const Options &Options::operator[](const std::string &name) const {
  TRACE("Options::operator[] const");
  
  if (!is_section) {
    throw BoutException(_("Option %s is not a section"), full_name.c_str());
  }

  if (name.empty()) {
    return *this;
  }

  // Find and return if already exists
  auto it = children.find(lowercase(name));
  if (it == children.end()) {
    // Doesn't exist
    throw BoutException(_("Option %s:%s does not exist"), full_name.c_str(), name.c_str());
  }

  return it->second;
}

Options& Options::operator=(const Options& other) {
  // Note: Here can't do copy-and-swap because pointers to parents are stored

  value = other.value;
  attributes = other.attributes;
  full_name = other.full_name;
  is_section = other.is_section;
  children = other.children;
  is_value = other.is_value;
  value_used = other.value_used;

  // Ensure that this is the parent of all children,
  // otherwise will point to the original Options instance
  for (auto& child : children) {
    child.second.parent_instance = this;
  }
  return *this;
}

bool Options::isSet() const {
  // Check if no value
  if (!is_value) {
    return false;
  }

  // Ignore if set from default
  if (bout::utils::variantEqualTo(attributes.at("source"), DEFAULT_SOURCE)) {
    return false;
  }

  return true;
}

bool Options::isSection(const std::string& name) const {
  if (name == "") {
    // Test this object
    return is_section;
  }

  // Is there a child section?
  auto it = children.find(lowercase(name));
  if (it == children.end()) {
    return false;
  } else {
    return it->second.isSection();
  }
}

template <>
void Options::assign<>(Field2D val, std::string source) {
  value = std::move(val);
  attributes["source"] = std::move(source);
  value_used = false;
  is_value = true;
}
template <>
void Options::assign<>(Field3D val, std::string source) {
  value = std::move(val);
  attributes["source"] = std::move(source);
  value_used = false;
  is_value = true;
}
template <>
void Options::assign<>(Array<BoutReal> val, std::string source) {
  value = std::move(val);
  attributes["source"] = std::move(source);
  value_used = false;
  is_value = true;
}
template <>
void Options::assign<>(Matrix<BoutReal> val, std::string source) {
  value = std::move(val);
  attributes["source"] = std::move(source);
  value_used = false;
  is_value = true;
}
template <>
void Options::assign<>(Tensor<BoutReal> val, std::string source) {
  value = std::move(val);
  attributes["source"] = std::move(source);
  value_used = false;
  is_value = true;
}

template <> std::string Options::as<std::string>(const std::string& UNUSED(similar_to)) const {
  if (!is_value) {
    throw BoutException(_("Option %s has no value"), full_name.c_str());
  }

  // Mark this option as used
  value_used = true;

  std::string result = bout::utils::variantToString(value);
  
  output_info << _("\tOption ") << full_name << " = " << result;
  if (attributes.count("source")) {
    // Specify the source of the setting
    output_info << " (" << bout::utils::variantToString(attributes.at("source")) << ")";
  }
  output_info << endl;

  return result;
}

template <> int Options::as<int>(const int& UNUSED(similar_to)) const {
  if (!is_value) {
    throw BoutException(_("Option %s has no value"), full_name.c_str());
  }

  int result;

  if (bout::utils::holds_alternative<int>(value)) {
    result = bout::utils::get<int>(value);
    
  } else {
    // Cases which get a BoutReal then check if close to an integer
    BoutReal rval;
    
    if (bout::utils::holds_alternative<BoutReal>(value)) {
      rval = bout::utils::get<BoutReal>(value);
    
    } else if (bout::utils::holds_alternative<std::string>(value)) {
      // Use FieldFactory to evaluate expression
      // Parse the string, giving this Option pointer for the context
      // then generate a value at t,x,y,z = 0,0,0,0
      auto gen = FieldFactory::get()->parse(bout::utils::get<std::string>(value), this);
      if (!gen) {
        throw BoutException(_("Couldn't get integer from option %s = '%s'"),
                            full_name.c_str(), bout::utils::variantToString(value).c_str());
      }
      rval = gen->generate(0, 0, 0, 0);
    } else {
      // Another type which can't be converted
      throw BoutException(_("Value for option %s is not an integer"),
                            full_name.c_str());
    }
    
    // Convert to int by rounding
    result = ROUND(rval);
    
    // Check that the value is close to an integer
    if (fabs(rval - static_cast<BoutReal>(result)) > 1e-3) {
      throw BoutException(_("Value for option %s = %e is not an integer"),
                          full_name.c_str(), rval);
    }
  }

  value_used = true;

  output_info << _("\tOption ") << full_name << " = " << result;
  if (attributes.count("source")) {
    // Specify the source of the setting
    output_info << " (" << bout::utils::variantToString(attributes.at("source")) << ")";
  }
  output_info << endl;

  return result;
}

template <> BoutReal Options::as<BoutReal>(const BoutReal& UNUSED(similar_to)) const {
  if (!is_value) {
    throw BoutException(_("Option %s has no value"), full_name.c_str());
  }

  BoutReal result;
  
  if (bout::utils::holds_alternative<int>(value)) {
    result = static_cast<BoutReal>(bout::utils::get<int>(value));
    
  } else if (bout::utils::holds_alternative<BoutReal>(value)) {
    result = bout::utils::get<BoutReal>(value);
      
  } else if (bout::utils::holds_alternative<std::string>(value)) {
    
    // Use FieldFactory to evaluate expression
    // Parse the string, giving this Option pointer for the context
    // then generate a value at t,x,y,z = 0,0,0,0
    auto gen = FieldFactory::get()->parse(bout::utils::get<std::string>(value), this);
    if (!gen) {
      throw BoutException(_("Couldn't get BoutReal from option %s = '%s'"), full_name.c_str(),
                          bout::utils::get<std::string>(value).c_str());
    }
    result = gen->generate(0, 0, 0, 0);
  } else {
    throw BoutException(_("Value for option %s cannot be converted to a BoutReal"),
                        full_name.c_str());
  }
  
  // Mark this option as used
  value_used = true;
  
  output_info << _("\tOption ") << full_name << " = " << result;
  if (attributes.count("source")) {
    // Specify the source of the setting
    output_info << " (" << bout::utils::variantToString(attributes.at("source")) << ")";
  }
  output_info << endl;
  
  return result;
}

template <> bool Options::as<bool>(const bool& UNUSED(similar_to)) const {
  if (!is_value) {
    throw BoutException(_("Option %s has no value"), full_name.c_str());
  }
  
  bool result;
  
  if (bout::utils::holds_alternative<bool>(value)) {
    result = bout::utils::get<bool>(value);
  
  } else if(bout::utils::holds_alternative<std::string>(value)) {
    auto strvalue = bout::utils::get<std::string>(value);
  
    auto c = static_cast<char>(toupper((strvalue)[0]));
    if ((c == 'Y') || (c == 'T') || (c == '1')) {
      result = true;
    } else if ((c == 'N') || (c == 'F') || (c == '0')) {
      result = false;
    } else {
      throw BoutException(_("\tOption '%s': Boolean expected. Got '%s'\n"), full_name.c_str(),
                          strvalue.c_str());
    }
  } else {
    throw BoutException(_("Value for option %s cannot be converted to a bool"),
                        full_name.c_str());
  }
  
  value_used = true;
  
  output_info << _("\tOption ") << full_name << " = " << toString(result);
  
  if (attributes.count("source")) {
    // Specify the source of the setting
    output_info << " (" << bout::utils::variantToString(attributes.at("source")) << ")";
  }
  output_info << endl;

  return result;
}

template <> Field3D Options::as<Field3D>(const Field3D& similar_to) const {
  if (!is_value) {
    throw BoutException("Option %s has no value", full_name.c_str());
  }

  // Mark value as used
  value_used = true;

  if (bout::utils::holds_alternative<Field3D>(value)) {
    Field3D stored_value = bout::utils::get<Field3D>(value);
    
    // Check that meta-data is consistent
    ASSERT1(areFieldsCompatible(stored_value, similar_to));
    
    return stored_value;
  }

  if (bout::utils::holds_alternative<Field2D>(value)) {
    const auto& stored_value = bout::utils::get<Field2D>(value);

    // Check that meta-data is consistent
    ASSERT1(areFieldsCompatible(stored_value, similar_to));

    return Field3D(stored_value);
  }
  
  try {
    BoutReal scalar_value = bout::utils::variantStaticCastOrThrow<ValueType, BoutReal>(value);
    
    // Get metadata from similar_to, fill field with scalar_value
    return filledFrom(similar_to, scalar_value);
  } catch (const std::bad_cast&) {
    
    // Convert from a string using FieldFactory
    if (bout::utils::holds_alternative<std::string>(value)) {
      return FieldFactory::get()->create3D(bout::utils::get<std::string>(value), this,
                                           similar_to.getMesh(),
                                           similar_to.getLocation());
    } else if (bout::utils::holds_alternative<Tensor<BoutReal>>(value)) {
      auto localmesh = similar_to.getMesh();
      if (!localmesh) {
        throw BoutException("mesh must be supplied when converting Tensor to Field3D");
      }

      // Get a reference, to try and avoid copying
      const auto& tensor = bout::utils::get<Tensor<BoutReal>>(value);
      
      // Check if the dimension sizes are the same as a Field3D
      if (tensor.shape() == std::make_tuple(localmesh->LocalNx,
                                            localmesh->LocalNy,
                                            localmesh->LocalNz)) {
        return Field3D(tensor.getData(), localmesh, similar_to.getLocation(),
                       {similar_to.getDirectionY(), similar_to.getDirectionZ()});
      }
      // If dimension sizes not the same, may be able
      // to select a region from it using Mesh e.g. if this
      // is from the input grid file.

    }
  }
  throw BoutException(_("Value for option %s cannot be converted to a Field3D"),
                      full_name.c_str());
}

template <> Field2D Options::as<Field2D>(const Field2D& similar_to) const {
  if (!is_value) {
    throw BoutException("Option %s has no value", full_name.c_str());
  }
  
  // Mark value as used
  value_used = true;

  if (bout::utils::holds_alternative<Field2D>(value)) {
    Field2D stored_value = bout::utils::get<Field2D>(value);
    
    // Check that meta-data is consistent
    ASSERT1(areFieldsCompatible(stored_value, similar_to));

    return stored_value;
  }
  
  try {
    BoutReal scalar_value = bout::utils::variantStaticCastOrThrow<ValueType, BoutReal>(value);

    // Get metadata from similar_to, fill field with scalar_value
    return filledFrom(similar_to, scalar_value);
  } catch (const std::bad_cast&) {
    
    // Convert from a string using FieldFactory
    if (bout::utils::holds_alternative<std::string>(value)) {
      return FieldFactory::get()->create2D(bout::utils::get<std::string>(value), this,
                                           similar_to.getMesh(),
                                           similar_to.getLocation());
    } else if (bout::utils::holds_alternative<Matrix<BoutReal>>(value)) {
      auto localmesh = similar_to.getMesh();
      if (!localmesh) {
        throw BoutException("mesh must be supplied when converting Matrix to Field2D");
      }

      // Get a reference, to try and avoid copying
      const auto& matrix = bout::utils::get<Matrix<BoutReal>>(value);

      // Check if the dimension sizes are the same as a Field3D
      if (matrix.shape() == std::make_tuple(localmesh->LocalNx,
                                            localmesh->LocalNy)) {
        return Field2D(matrix.getData(), localmesh, similar_to.getLocation(),
                       {similar_to.getDirectionY(), similar_to.getDirectionZ()});
      }
    }
  }
  throw BoutException(_("Value for option %s cannot be converted to a Field2D"),
                      full_name.c_str());
}

// Note: This is defined here rather than in the header
// to avoid using as<string> before specialising it.
bool Options::operator==(const char* other) const {
  return as<std::string>() == std::string(other);
}

bool Options::operator<(const char* other) const {
  return as<std::string>() < std::string(other);
}

void Options::printUnused() const {
  bool allused = true;
  // Check if any options are unused
  for (const auto &it : children) {
    if (it.second.is_value && !it.second.value_used) {
      allused = false;
      break;
    }
  }
  if (allused) {
    output_info << _("All options used\n");
  } else {
    output_info << _("Unused options:\n");
    for (const auto &it : children) {
      if (it.second.is_value && !it.second.value_used) {
        output_info << "\t" << full_name << ":" << it.first << " = "
                    << bout::utils::variantToString(it.second.value);
        if (it.second.attributes.count("source"))
          output_info << " (" << bout::utils::variantToString(it.second.attributes.at("source")) << ")";
        output_info << endl;
      }
    }
  }
  for (const auto &it : children) {
    if (it.second.is_section) {
      it.second.printUnused();
    }
  }
}

void Options::cleanCache() { FieldFactory::get()->cleanCache(); }

std::map<std::string, Options::OptionValue> Options::values() const {
  std::map<std::string, OptionValue> options;
  for (const auto& it : children) {
    if (it.second.is_value) {
      options.emplace(it.first, OptionValue { bout::utils::variantToString(it.second.value),
                                               bout::utils::variantToString(it.second.attributes.at("source")),
                                               it.second.value_used});
    }
  }
  return options;
}

std::map<std::string, const Options *> Options::subsections() const {
  std::map<std::string, const Options *> sections;
  for (const auto &it : children) {
    if (it.second.is_section) {
      sections[it.first] = &it.second;
    }
  }
  return sections;
}
