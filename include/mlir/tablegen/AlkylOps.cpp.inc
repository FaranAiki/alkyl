/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|* Op Definitions                                                             *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|* From: AlkylOps.td                                                          *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifdef GET_OP_LIST
#undef GET_OP_LIST

::alkyl_mlir::ClassAllocOp,
::alkyl_mlir::EnumMatchOp,
::alkyl_mlir::PristineOp,
::alkyl_mlir::PureCallOp,
::alkyl_mlir::TaintOp
#endif // GET_OP_LIST

#ifdef GET_OP_CLASSES
#undef GET_OP_CLASSES


//===----------------------------------------------------------------------===//
// Local Utility Method Definitions
//===----------------------------------------------------------------------===//


static ::llvm::LogicalResult __mlir_ods_local_type_constraint_AlkylOps1(
    ::mlir::Operation *op, ::mlir::Type type, ::llvm::StringRef valueKind,
    unsigned valueIndex) {
  if (!((true))) {
    return op->emitOpError(valueKind) << " #" << valueIndex
        << " must be any type, but got " << type;
  }
  return ::mlir::success();
}

static ::llvm::LogicalResult __mlir_ods_local_type_constraint_AlkylOps2(
    ::mlir::Operation *op, ::mlir::Type type, ::llvm::StringRef valueKind,
    unsigned valueIndex) {
  if (!((type.isSignlessInteger(1)))) {
    return op->emitOpError(valueKind) << " #" << valueIndex
        << " must be 1-bit signless integer, but got " << type;
  }
  return ::mlir::success();
}

static ::llvm::LogicalResult __mlir_ods_local_type_constraint_AlkylOps3(
    ::mlir::Operation *op, ::mlir::Type type, ::llvm::StringRef valueKind,
    unsigned valueIndex) {
  if (!((true))) {
    return op->emitOpError(valueKind) << " #" << valueIndex
        << " must be variadic of any type, but got " << type;
  }
  return ::mlir::success();
}

static ::llvm::LogicalResult __mlir_ods_local_attr_constraint_AlkylOps1(
    ::mlir::Attribute attr, ::llvm::StringRef attrName, llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  if (attr && !((::llvm::isa<::mlir::StringAttr>(attr))))
    return emitError() << "attribute '" << attrName
        << "' failed to satisfy constraint: string attribute";
  return ::mlir::success();
}
static ::llvm::LogicalResult __mlir_ods_local_attr_constraint_AlkylOps1(
    ::mlir::Operation *op, ::mlir::Attribute attr, ::llvm::StringRef attrName) {
  return __mlir_ods_local_attr_constraint_AlkylOps1(attr, attrName, [op]() {
    return op->emitOpError();
  });
}

static ::llvm::LogicalResult __mlir_ods_local_attr_constraint_AlkylOps2(
    ::mlir::Attribute attr, ::llvm::StringRef attrName, llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  if (attr && !(((::llvm::isa<::mlir::IntegerAttr>(attr))) && ((::llvm::cast<::mlir::IntegerAttr>(attr).getType().isSignlessInteger(32)))))
    return emitError() << "attribute '" << attrName
        << "' failed to satisfy constraint: 32-bit signless integer attribute";
  return ::mlir::success();
}
static ::llvm::LogicalResult __mlir_ods_local_attr_constraint_AlkylOps2(
    ::mlir::Operation *op, ::mlir::Attribute attr, ::llvm::StringRef attrName) {
  return __mlir_ods_local_attr_constraint_AlkylOps2(attr, attrName, [op]() {
    return op->emitOpError();
  });
}

static ::llvm::LogicalResult __mlir_ods_local_attr_constraint_AlkylOps3(
    ::mlir::Attribute attr, ::llvm::StringRef attrName, llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  if (attr && !((::llvm::isa<::mlir::SymbolRefAttr>(attr))))
    return emitError() << "attribute '" << attrName
        << "' failed to satisfy constraint: symbol reference attribute";
  return ::mlir::success();
}
static ::llvm::LogicalResult __mlir_ods_local_attr_constraint_AlkylOps3(
    ::mlir::Operation *op, ::mlir::Attribute attr, ::llvm::StringRef attrName) {
  return __mlir_ods_local_attr_constraint_AlkylOps3(attr, attrName, [op]() {
    return op->emitOpError();
  });
}
namespace alkyl_mlir {


//===----------------------------------------------------------------------===//
// ::alkyl_mlir::ClassAllocOp definitions
//===----------------------------------------------------------------------===//

namespace detail {

ClassAllocOpGenericAdaptorBase::ClassAllocOpGenericAdaptorBase(ClassAllocOp op) : odsAttrs(op->getRawDictionaryAttrs()), odsOpName(op->getName()), properties(op.getProperties()), odsRegions(op->getRegions()) {}

::llvm::StringRef ClassAllocOpGenericAdaptorBase::getClassName() {
  auto attr = getClassNameAttr();
  return attr.getValue();
}


} // namespace detail
ClassAllocOpAdaptor::ClassAllocOpAdaptor(ClassAllocOp op) : ClassAllocOpGenericAdaptor(op->getOperands(), op) {}

::llvm::LogicalResult ClassAllocOpAdaptor::verify(::mlir::Location loc) {
  auto tblgen_class_name = getProperties().class_name; (void)tblgen_class_name;
  if (!tblgen_class_name) return emitError(loc, "'alkyl.class_alloc' op requires attribute 'class_name'");

  if (tblgen_class_name && !((::llvm::isa<::mlir::StringAttr>(tblgen_class_name))))
    return emitError(loc, "'alkyl.class_alloc' op attribute 'class_name' failed to satisfy constraint: string attribute");
  return ::mlir::success();
}

::llvm::LogicalResult ClassAllocOp::setPropertiesFromAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }

  {
    auto &propStorage = prop.class_name;
       auto attr = dict.get("class_name");
    if (attr) {
      auto convertedAttr = ::llvm::dyn_cast<std::remove_reference_t<decltype(propStorage)>>(attr);
      if (convertedAttr) {
        propStorage = convertedAttr;
      } else {
        emitError() << "Invalid attribute `class_name` in property conversion: " << attr;
        return ::mlir::failure();
      }
    }
  }
  return ::mlir::success();
}

::mlir::Attribute ClassAllocOp::getPropertiesAsAttr(::mlir::MLIRContext *ctx, const Properties &prop) {
    ::mlir::SmallVector<::mlir::NamedAttribute> attrs;
    ::mlir::Builder odsBuilder{ctx};

    {
      const auto &propStorage = prop.class_name;
      if (propStorage)
        attrs.push_back(odsBuilder.getNamedAttr("class_name",
                                       propStorage));
    }

  if (!attrs.empty())
    return odsBuilder.getDictionaryAttr(attrs);
  return {};
}

llvm::hash_code ClassAllocOp::computePropertiesHash(const Properties &prop) {
  using llvm::hash_value;
  return llvm::hash_combine(
    llvm::hash_value(prop.class_name.getAsOpaquePointer()));
}

std::optional<mlir::Attribute> ClassAllocOp::getInherentAttr(::mlir::MLIRContext *ctx, const Properties &prop, llvm::StringRef name) {
    if (name == "class_name")
      return prop.class_name;
  return std::nullopt;
}

void ClassAllocOp::setInherentAttr(Properties &prop, llvm::StringRef name, mlir::Attribute value) {
    if (name == "class_name") {
       prop.class_name = ::llvm::dyn_cast_or_null<std::remove_reference_t<decltype(prop.class_name)>>(value);
       return;
    }
}

void ClassAllocOp::populateInherentAttrs(::mlir::MLIRContext *ctx, const Properties &prop, ::mlir::NamedAttrList &attrs) {
    if (prop.class_name) attrs.append("class_name", prop.class_name);
}

::llvm::LogicalResult ClassAllocOp::verifyInherentAttrs(::mlir::OperationName opName, ::mlir::NamedAttrList &attrs, llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
    {
      ::mlir::Attribute attr = attrs.get(getClassNameAttrName(opName));
      if (attr && ::mlir::failed(__mlir_ods_local_attr_constraint_AlkylOps1(attr, "class_name", emitError)))
        return ::mlir::failure();
    }
    return ::mlir::success();
}

::llvm::LogicalResult ClassAllocOp::readProperties(::mlir::DialectBytecodeReader &reader, ::mlir::OperationState &state) {
  auto &prop = state.getOrAddProperties<Properties>(); (void)prop;
  if (::mlir::failed(reader.readAttribute(prop.class_name)))
    return ::mlir::failure();
  return ::mlir::success();
}

void ClassAllocOp::writeProperties(::mlir::DialectBytecodeWriter &writer) {
  auto &prop = getProperties(); (void)prop;
  writer.writeAttribute(prop.class_name);
}

::llvm::StringRef ClassAllocOp::getClassName() {
  auto attr = getClassNameAttr();
  return attr.getValue();
}

void ClassAllocOp::setClassName(::llvm::StringRef attrValue) {
  getProperties().class_name = ::mlir::Builder((*this)->getContext()).getStringAttr(attrValue);
}

void ClassAllocOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::Type instance, ::mlir::StringAttr class_name) {
  odsState.getOrAddProperties<Properties>().class_name = class_name;
  odsState.addTypes(instance);
}

ClassAllocOp ClassAllocOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::Type instance, ::mlir::StringAttr class_name) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(instance)>(instance), std::forward<decltype(class_name)>(class_name));
  auto __res__ = ::llvm::dyn_cast<ClassAllocOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

ClassAllocOp ClassAllocOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::Type instance, ::mlir::StringAttr class_name) {
  return create(builder, builder.getLoc(), std::forward<decltype(instance)>(instance), std::forward<decltype(class_name)>(class_name));
}

void ClassAllocOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::StringAttr class_name) {
  odsState.getOrAddProperties<Properties>().class_name = class_name;
  assert(resultTypes.size() == 1u && "mismatched number of results");
  odsState.addTypes(resultTypes);
}

ClassAllocOp ClassAllocOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::StringAttr class_name) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(class_name)>(class_name));
  auto __res__ = ::llvm::dyn_cast<ClassAllocOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

ClassAllocOp ClassAllocOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::StringAttr class_name) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(class_name)>(class_name));
}

void ClassAllocOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::Type instance, ::llvm::StringRef class_name) {
  odsState.getOrAddProperties<Properties>().class_name = odsBuilder.getStringAttr(class_name);
  odsState.addTypes(instance);
}

ClassAllocOp ClassAllocOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::Type instance, ::llvm::StringRef class_name) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(instance)>(instance), std::forward<decltype(class_name)>(class_name));
  auto __res__ = ::llvm::dyn_cast<ClassAllocOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

ClassAllocOp ClassAllocOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::Type instance, ::llvm::StringRef class_name) {
  return create(builder, builder.getLoc(), std::forward<decltype(instance)>(instance), std::forward<decltype(class_name)>(class_name));
}

void ClassAllocOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::llvm::StringRef class_name) {
  odsState.getOrAddProperties<Properties>().class_name = odsBuilder.getStringAttr(class_name);
  assert(resultTypes.size() == 1u && "mismatched number of results");
  odsState.addTypes(resultTypes);
}

ClassAllocOp ClassAllocOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::llvm::StringRef class_name) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(class_name)>(class_name));
  auto __res__ = ::llvm::dyn_cast<ClassAllocOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

ClassAllocOp ClassAllocOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::llvm::StringRef class_name) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(class_name)>(class_name));
}

void ClassAllocOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  assert(operands.size() == 0u && "mismatched number of parameters");
  odsState.addOperands(operands);
  odsState.addAttributes(attributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);

  if (!attributes.empty()) {
    ::mlir::OpaqueProperties properties =
      &odsState.getOrAddProperties<ClassAllocOp::Properties>();
    std::optional<::mlir::RegisteredOperationName> info =
      odsState.name.getRegisteredInfo();
    if (failed(info->setOpPropertiesFromAttribute(odsState.name, properties,
        odsState.attributes.getDictionary(odsState.getContext()), nullptr)))
      ::llvm::report_fatal_error("Property conversion failed.");
  }
}

ClassAllocOp ClassAllocOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
  auto __res__ = ::llvm::dyn_cast<ClassAllocOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

ClassAllocOp ClassAllocOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
}

void ClassAllocOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  assert(operands.size() == 0u && "mismatched number of parameters");
  odsState.addOperands(operands);
  odsState.useProperties(const_cast<Properties&>(properties));
  odsState.addAttributes(discardableAttributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);
}

ClassAllocOp ClassAllocOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
  auto __res__ = ::llvm::dyn_cast<ClassAllocOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

ClassAllocOp ClassAllocOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
}

::llvm::LogicalResult ClassAllocOp::verifyInvariantsImpl() {
  auto tblgen_class_name = getProperties().class_name; (void)tblgen_class_name;
  if (!tblgen_class_name) return emitOpError("requires attribute 'class_name'");

  if (::mlir::failed(__mlir_ods_local_attr_constraint_AlkylOps1(*this, tblgen_class_name, "class_name")))
    return ::mlir::failure();
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSResults(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps1(*this, v.getType(), "result", index++)))
        return ::mlir::failure();
    }
  }
  return ::mlir::success();
}

::llvm::LogicalResult ClassAllocOp::verifyInvariants() {
  return verifyInvariantsImpl();
}

::llvm::LogicalResult ClassAllocOp::setPropertiesFromParsedAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }
  // keep track of used keys in the input dictionary to be able to error out
  // if there are some unknown ones.
  ::mlir::DenseSet<::mlir::StringAttr> usedKeys;
  ::mlir::MLIRContext *ctx = dict.getContext();
  (void)ctx;
  {

    auto &propStorage = prop.class_name;
    auto class_nameAttrName = ::mlir::StringAttr::get(ctx, "class_name");
    auto attr = dict.get(class_nameAttrName);
    usedKeys.insert(class_nameAttrName);
    if (attr || /*isRequired=*/true) {
      if (!attr) {
        emitError() << "expected key entry for class_name in DictionaryAttr to set "
                   "Properties.";
        return ::mlir::failure();
      }
      auto convertedAttr = ::llvm::dyn_cast<std::remove_reference_t<decltype(propStorage)>>(attr);
      if (convertedAttr) {
        propStorage = convertedAttr;
      } else {
        emitError() << "Invalid attribute `class_name` in property conversion: " << attr;
        return ::mlir::failure();
      }
    }
  }

  for (::mlir::NamedAttribute attr : dict) {
    if (!usedKeys.contains(attr.getName()))
      return emitError() << "unknown key '" << attr.getName() <<
          "' when parsing properties dictionary";
  }
  return ::mlir::success();
}


} // namespace alkyl_mlir
MLIR_DEFINE_EXPLICIT_TYPE_ID(::alkyl_mlir::ClassAllocOp)

namespace alkyl_mlir {


//===----------------------------------------------------------------------===//
// ::alkyl_mlir::EnumMatchOp definitions
//===----------------------------------------------------------------------===//

namespace detail {

EnumMatchOpGenericAdaptorBase::EnumMatchOpGenericAdaptorBase(EnumMatchOp op) : odsAttrs(op->getRawDictionaryAttrs()), odsOpName(op->getName()), properties(op.getProperties()), odsRegions(op->getRegions()) {}

uint32_t EnumMatchOpGenericAdaptorBase::getVariantTag() {
  auto attr = getVariantTagAttr();
  return attr.getValue().getZExtValue();
}


} // namespace detail
EnumMatchOpAdaptor::EnumMatchOpAdaptor(EnumMatchOp op) : EnumMatchOpGenericAdaptor(op->getOperands(), op) {}

::llvm::LogicalResult EnumMatchOpAdaptor::verify(::mlir::Location loc) {
  auto tblgen_variant_tag = getProperties().variant_tag; (void)tblgen_variant_tag;
  if (!tblgen_variant_tag) return emitError(loc, "'alkyl.enum_match' op requires attribute 'variant_tag'");

  if (tblgen_variant_tag && !(((::llvm::isa<::mlir::IntegerAttr>(tblgen_variant_tag))) && ((::llvm::cast<::mlir::IntegerAttr>(tblgen_variant_tag).getType().isSignlessInteger(32)))))
    return emitError(loc, "'alkyl.enum_match' op attribute 'variant_tag' failed to satisfy constraint: 32-bit signless integer attribute");
  return ::mlir::success();
}

::llvm::LogicalResult EnumMatchOp::setPropertiesFromAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }

  {
    auto &propStorage = prop.variant_tag;
       auto attr = dict.get("variant_tag");
    if (attr) {
      auto convertedAttr = ::llvm::dyn_cast<std::remove_reference_t<decltype(propStorage)>>(attr);
      if (convertedAttr) {
        propStorage = convertedAttr;
      } else {
        emitError() << "Invalid attribute `variant_tag` in property conversion: " << attr;
        return ::mlir::failure();
      }
    }
  }
  return ::mlir::success();
}

::mlir::Attribute EnumMatchOp::getPropertiesAsAttr(::mlir::MLIRContext *ctx, const Properties &prop) {
    ::mlir::SmallVector<::mlir::NamedAttribute> attrs;
    ::mlir::Builder odsBuilder{ctx};

    {
      const auto &propStorage = prop.variant_tag;
      if (propStorage)
        attrs.push_back(odsBuilder.getNamedAttr("variant_tag",
                                       propStorage));
    }

  if (!attrs.empty())
    return odsBuilder.getDictionaryAttr(attrs);
  return {};
}

llvm::hash_code EnumMatchOp::computePropertiesHash(const Properties &prop) {
  using llvm::hash_value;
  return llvm::hash_combine(
    llvm::hash_value(prop.variant_tag.getAsOpaquePointer()));
}

std::optional<mlir::Attribute> EnumMatchOp::getInherentAttr(::mlir::MLIRContext *ctx, const Properties &prop, llvm::StringRef name) {
    if (name == "variant_tag")
      return prop.variant_tag;
  return std::nullopt;
}

void EnumMatchOp::setInherentAttr(Properties &prop, llvm::StringRef name, mlir::Attribute value) {
    if (name == "variant_tag") {
       prop.variant_tag = ::llvm::dyn_cast_or_null<std::remove_reference_t<decltype(prop.variant_tag)>>(value);
       return;
    }
}

void EnumMatchOp::populateInherentAttrs(::mlir::MLIRContext *ctx, const Properties &prop, ::mlir::NamedAttrList &attrs) {
    if (prop.variant_tag) attrs.append("variant_tag", prop.variant_tag);
}

::llvm::LogicalResult EnumMatchOp::verifyInherentAttrs(::mlir::OperationName opName, ::mlir::NamedAttrList &attrs, llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
    {
      ::mlir::Attribute attr = attrs.get(getVariantTagAttrName(opName));
      if (attr && ::mlir::failed(__mlir_ods_local_attr_constraint_AlkylOps2(attr, "variant_tag", emitError)))
        return ::mlir::failure();
    }
    return ::mlir::success();
}

::llvm::LogicalResult EnumMatchOp::readProperties(::mlir::DialectBytecodeReader &reader, ::mlir::OperationState &state) {
  auto &prop = state.getOrAddProperties<Properties>(); (void)prop;
  if (::mlir::failed(reader.readAttribute(prop.variant_tag)))
    return ::mlir::failure();
  return ::mlir::success();
}

void EnumMatchOp::writeProperties(::mlir::DialectBytecodeWriter &writer) {
  auto &prop = getProperties(); (void)prop;
  writer.writeAttribute(prop.variant_tag);
}

uint32_t EnumMatchOp::getVariantTag() {
  auto attr = getVariantTagAttr();
  return attr.getValue().getZExtValue();
}

void EnumMatchOp::setVariantTag(uint32_t attrValue) {
  getProperties().variant_tag = ::mlir::Builder((*this)->getContext()).getIntegerAttr(::mlir::Builder((*this)->getContext()).getIntegerType(32), attrValue);
}

void EnumMatchOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::Type is_match, ::mlir::Value enum_val, ::mlir::IntegerAttr variant_tag) {
  odsState.addOperands(enum_val);
  odsState.getOrAddProperties<Properties>().variant_tag = variant_tag;
  odsState.addTypes(is_match);
}

EnumMatchOp EnumMatchOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::Type is_match, ::mlir::Value enum_val, ::mlir::IntegerAttr variant_tag) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(is_match)>(is_match), std::forward<decltype(enum_val)>(enum_val), std::forward<decltype(variant_tag)>(variant_tag));
  auto __res__ = ::llvm::dyn_cast<EnumMatchOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

EnumMatchOp EnumMatchOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::Type is_match, ::mlir::Value enum_val, ::mlir::IntegerAttr variant_tag) {
  return create(builder, builder.getLoc(), std::forward<decltype(is_match)>(is_match), std::forward<decltype(enum_val)>(enum_val), std::forward<decltype(variant_tag)>(variant_tag));
}

void EnumMatchOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::Value enum_val, ::mlir::IntegerAttr variant_tag) {
  odsState.addOperands(enum_val);
  odsState.getOrAddProperties<Properties>().variant_tag = variant_tag;
  assert(resultTypes.size() == 1u && "mismatched number of results");
  odsState.addTypes(resultTypes);
}

EnumMatchOp EnumMatchOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::Value enum_val, ::mlir::IntegerAttr variant_tag) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(enum_val)>(enum_val), std::forward<decltype(variant_tag)>(variant_tag));
  auto __res__ = ::llvm::dyn_cast<EnumMatchOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

EnumMatchOp EnumMatchOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::Value enum_val, ::mlir::IntegerAttr variant_tag) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(enum_val)>(enum_val), std::forward<decltype(variant_tag)>(variant_tag));
}

void EnumMatchOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::Type is_match, ::mlir::Value enum_val, uint32_t variant_tag) {
  odsState.addOperands(enum_val);
  odsState.getOrAddProperties<Properties>().variant_tag = odsBuilder.getIntegerAttr(odsBuilder.getIntegerType(32), variant_tag);
  odsState.addTypes(is_match);
}

EnumMatchOp EnumMatchOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::Type is_match, ::mlir::Value enum_val, uint32_t variant_tag) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(is_match)>(is_match), std::forward<decltype(enum_val)>(enum_val), std::forward<decltype(variant_tag)>(variant_tag));
  auto __res__ = ::llvm::dyn_cast<EnumMatchOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

EnumMatchOp EnumMatchOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::Type is_match, ::mlir::Value enum_val, uint32_t variant_tag) {
  return create(builder, builder.getLoc(), std::forward<decltype(is_match)>(is_match), std::forward<decltype(enum_val)>(enum_val), std::forward<decltype(variant_tag)>(variant_tag));
}

void EnumMatchOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::Value enum_val, uint32_t variant_tag) {
  odsState.addOperands(enum_val);
  odsState.getOrAddProperties<Properties>().variant_tag = odsBuilder.getIntegerAttr(odsBuilder.getIntegerType(32), variant_tag);
  assert(resultTypes.size() == 1u && "mismatched number of results");
  odsState.addTypes(resultTypes);
}

EnumMatchOp EnumMatchOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::Value enum_val, uint32_t variant_tag) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(enum_val)>(enum_val), std::forward<decltype(variant_tag)>(variant_tag));
  auto __res__ = ::llvm::dyn_cast<EnumMatchOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

EnumMatchOp EnumMatchOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::Value enum_val, uint32_t variant_tag) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(enum_val)>(enum_val), std::forward<decltype(variant_tag)>(variant_tag));
}

void EnumMatchOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  assert(operands.size() == 1u && "mismatched number of parameters");
  odsState.addOperands(operands);
  odsState.addAttributes(attributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);

  if (!attributes.empty()) {
    ::mlir::OpaqueProperties properties =
      &odsState.getOrAddProperties<EnumMatchOp::Properties>();
    std::optional<::mlir::RegisteredOperationName> info =
      odsState.name.getRegisteredInfo();
    if (failed(info->setOpPropertiesFromAttribute(odsState.name, properties,
        odsState.attributes.getDictionary(odsState.getContext()), nullptr)))
      ::llvm::report_fatal_error("Property conversion failed.");
  }
}

EnumMatchOp EnumMatchOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
  auto __res__ = ::llvm::dyn_cast<EnumMatchOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

EnumMatchOp EnumMatchOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
}

void EnumMatchOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  assert(operands.size() == 1u && "mismatched number of parameters");
  odsState.addOperands(operands);
  odsState.useProperties(const_cast<Properties&>(properties));
  odsState.addAttributes(discardableAttributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);
}

EnumMatchOp EnumMatchOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
  auto __res__ = ::llvm::dyn_cast<EnumMatchOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

EnumMatchOp EnumMatchOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
}

::llvm::LogicalResult EnumMatchOp::verifyInvariantsImpl() {
  auto tblgen_variant_tag = getProperties().variant_tag; (void)tblgen_variant_tag;
  if (!tblgen_variant_tag) return emitOpError("requires attribute 'variant_tag'");

  if (::mlir::failed(__mlir_ods_local_attr_constraint_AlkylOps2(*this, tblgen_variant_tag, "variant_tag")))
    return ::mlir::failure();
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSOperands(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps1(*this, v.getType(), "operand", index++)))
        return ::mlir::failure();
    }
  }
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSResults(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps2(*this, v.getType(), "result", index++)))
        return ::mlir::failure();
    }
  }
  return ::mlir::success();
}

::llvm::LogicalResult EnumMatchOp::verifyInvariants() {
  return verifyInvariantsImpl();
}

::llvm::LogicalResult EnumMatchOp::setPropertiesFromParsedAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }
  // keep track of used keys in the input dictionary to be able to error out
  // if there are some unknown ones.
  ::mlir::DenseSet<::mlir::StringAttr> usedKeys;
  ::mlir::MLIRContext *ctx = dict.getContext();
  (void)ctx;
  {

    auto &propStorage = prop.variant_tag;
    auto variant_tagAttrName = ::mlir::StringAttr::get(ctx, "variant_tag");
    auto attr = dict.get(variant_tagAttrName);
    usedKeys.insert(variant_tagAttrName);
    if (attr || /*isRequired=*/true) {
      if (!attr) {
        emitError() << "expected key entry for variant_tag in DictionaryAttr to set "
                   "Properties.";
        return ::mlir::failure();
      }
      auto convertedAttr = ::llvm::dyn_cast<std::remove_reference_t<decltype(propStorage)>>(attr);
      if (convertedAttr) {
        propStorage = convertedAttr;
      } else {
        emitError() << "Invalid attribute `variant_tag` in property conversion: " << attr;
        return ::mlir::failure();
      }
    }
  }

  for (::mlir::NamedAttribute attr : dict) {
    if (!usedKeys.contains(attr.getName()))
      return emitError() << "unknown key '" << attr.getName() <<
          "' when parsing properties dictionary";
  }
  return ::mlir::success();
}

void EnumMatchOp::getEffects(::llvm::SmallVectorImpl<::mlir::SideEffects::EffectInstance<::mlir::MemoryEffects::Effect>> &effects) {
}


} // namespace alkyl_mlir
MLIR_DEFINE_EXPLICIT_TYPE_ID(::alkyl_mlir::EnumMatchOp)

namespace alkyl_mlir {


//===----------------------------------------------------------------------===//
// ::alkyl_mlir::PristineOp definitions
//===----------------------------------------------------------------------===//

namespace detail {

PristineOpGenericAdaptorBase::PristineOpGenericAdaptorBase(PristineOp op) : odsAttrs(op->getRawDictionaryAttrs()), odsOpName(op->getName()), properties(op.getProperties()), odsRegions(op->getRegions()) {}


} // namespace detail
PristineOpAdaptor::PristineOpAdaptor(PristineOp op) : PristineOpGenericAdaptor(op->getOperands(), op) {}

::llvm::LogicalResult PristineOpAdaptor::verify(::mlir::Location loc) {
  return ::mlir::success();
}

::llvm::LogicalResult PristineOp::setPropertiesFromAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }
      return ::mlir::success();
}

::mlir::Attribute PristineOp::getPropertiesAsAttr(::mlir::MLIRContext *ctx, const Properties &prop) {
    ::mlir::SmallVector<::mlir::NamedAttribute> attrs;
    ::mlir::Builder odsBuilder{ctx};

  if (!attrs.empty())
    return odsBuilder.getDictionaryAttr(attrs);
  return {};
}

llvm::hash_code PristineOp::computePropertiesHash(const Properties &prop) {
  using llvm::hash_value;
  return llvm::hash_combine();
}

std::optional<mlir::Attribute> PristineOp::getInherentAttr(::mlir::MLIRContext *ctx, const Properties &prop, llvm::StringRef name) {
  return std::nullopt;
}

void PristineOp::setInherentAttr(Properties &prop, llvm::StringRef name, mlir::Attribute value) {
}

void PristineOp::populateInherentAttrs(::mlir::MLIRContext *ctx, const Properties &prop, ::mlir::NamedAttrList &attrs) {
}

::llvm::LogicalResult PristineOp::verifyInherentAttrs(::mlir::OperationName opName, ::mlir::NamedAttrList &attrs, llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
    return ::mlir::success();
}

void PristineOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::Type pristine_val, ::mlir::Value tainted_val) {
  odsState.addOperands(tainted_val);
  odsState.addTypes(pristine_val);
}

PristineOp PristineOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::Type pristine_val, ::mlir::Value tainted_val) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(pristine_val)>(pristine_val), std::forward<decltype(tainted_val)>(tainted_val));
  auto __res__ = ::llvm::dyn_cast<PristineOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

PristineOp PristineOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::Type pristine_val, ::mlir::Value tainted_val) {
  return create(builder, builder.getLoc(), std::forward<decltype(pristine_val)>(pristine_val), std::forward<decltype(tainted_val)>(tainted_val));
}

void PristineOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::Value tainted_val) {
  odsState.addOperands(tainted_val);
  assert(resultTypes.size() == 1u && "mismatched number of results");
  odsState.addTypes(resultTypes);
}

PristineOp PristineOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::Value tainted_val) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(tainted_val)>(tainted_val));
  auto __res__ = ::llvm::dyn_cast<PristineOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

PristineOp PristineOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::Value tainted_val) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(tainted_val)>(tainted_val));
}

void PristineOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  assert(operands.size() == 1u && "mismatched number of parameters");
  odsState.addOperands(operands);
  odsState.addAttributes(attributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);
}

PristineOp PristineOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
  auto __res__ = ::llvm::dyn_cast<PristineOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

PristineOp PristineOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
}

void PristineOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  assert(operands.size() == 1u && "mismatched number of parameters");
  odsState.addOperands(operands);
  odsState.useProperties(const_cast<Properties&>(properties));
  odsState.addAttributes(discardableAttributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);
}

PristineOp PristineOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
  auto __res__ = ::llvm::dyn_cast<PristineOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

PristineOp PristineOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
}

::llvm::LogicalResult PristineOp::verifyInvariantsImpl() {
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSOperands(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps1(*this, v.getType(), "operand", index++)))
        return ::mlir::failure();
    }
  }
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSResults(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps1(*this, v.getType(), "result", index++)))
        return ::mlir::failure();
    }
  }
  return ::mlir::success();
}

::llvm::LogicalResult PristineOp::verifyInvariants() {
  return verifyInvariantsImpl();
}

::llvm::LogicalResult PristineOp::setPropertiesFromParsedAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }
  // keep track of used keys in the input dictionary to be able to error out
  // if there are some unknown ones.
  ::mlir::DenseSet<::mlir::StringAttr> usedKeys;
  ::mlir::MLIRContext *ctx = dict.getContext();
  (void)ctx;

  for (::mlir::NamedAttribute attr : dict) {
    if (!usedKeys.contains(attr.getName()))
      return emitError() << "unknown key '" << attr.getName() <<
          "' when parsing properties dictionary";
  }
  return ::mlir::success();
}

void PristineOp::getEffects(::llvm::SmallVectorImpl<::mlir::SideEffects::EffectInstance<::mlir::MemoryEffects::Effect>> &effects) {
}


} // namespace alkyl_mlir
MLIR_DEFINE_EXPLICIT_TYPE_ID(::alkyl_mlir::PristineOp)

namespace alkyl_mlir {


//===----------------------------------------------------------------------===//
// ::alkyl_mlir::PureCallOp definitions
//===----------------------------------------------------------------------===//

namespace detail {

PureCallOpGenericAdaptorBase::PureCallOpGenericAdaptorBase(PureCallOp op) : odsAttrs(op->getRawDictionaryAttrs()), odsOpName(op->getName()), properties(op.getProperties()), odsRegions(op->getRegions()) {}

std::pair<unsigned, unsigned> PureCallOpGenericAdaptorBase::getODSOperandIndexAndLength(unsigned index, unsigned odsOperandsSize) {
  bool isVariadic[] = {true};
  int prevVariadicCount = 0;
  for (unsigned i = 0; i < index; ++i)
    if (isVariadic[i]) ++prevVariadicCount;

  // Calculate how many dynamic values a static variadic operand corresponds to.
  // This assumes all static variadic operands have the same dynamic value count.
  int variadicSize = (odsOperandsSize - 0) / 1;
  // `index` passed in as the parameter is the static index which counts each
  // operand (variadic or not) as size 1. So here for each previous static variadic
  // operand, we need to offset by (variadicSize - 1) to get where the dynamic
  // value pack for this static operand starts.
  int start = index + (variadicSize - 1) * prevVariadicCount;
  int size = isVariadic[index] ? variadicSize : 1;
  return {start, size};
}

::mlir::SymbolRefAttr PureCallOpGenericAdaptorBase::getCallee() {
  auto attr = getCalleeAttr();
  return attr;
}


} // namespace detail
PureCallOpAdaptor::PureCallOpAdaptor(PureCallOp op) : PureCallOpGenericAdaptor(op->getOperands(), op) {}

::llvm::LogicalResult PureCallOpAdaptor::verify(::mlir::Location loc) {
  auto tblgen_callee = getProperties().callee; (void)tblgen_callee;
  if (!tblgen_callee) return emitError(loc, "'alkyl.call_pure' op requires attribute 'callee'");

  if (tblgen_callee && !((::llvm::isa<::mlir::SymbolRefAttr>(tblgen_callee))))
    return emitError(loc, "'alkyl.call_pure' op attribute 'callee' failed to satisfy constraint: symbol reference attribute");
  return ::mlir::success();
}

std::pair<unsigned, unsigned> PureCallOp::getODSOperandIndexAndLength(unsigned index) {
  bool isVariadic[] = {true};
  int prevVariadicCount = 0;
  for (unsigned i = 0; i < index; ++i)
    if (isVariadic[i]) ++prevVariadicCount;

  // Calculate how many dynamic values a static variadic operand corresponds to.
  // This assumes all static variadic operands have the same dynamic value count.
  int variadicSize = (getOperation()->getNumOperands() - 0) / 1;
  // `index` passed in as the parameter is the static index which counts each
  // operand (variadic or not) as size 1. So here for each previous static variadic
  // operand, we need to offset by (variadicSize - 1) to get where the dynamic
  // value pack for this static operand starts.
  int start = index + (variadicSize - 1) * prevVariadicCount;
  int size = isVariadic[index] ? variadicSize : 1;
  return {start, size};
}

::mlir::MutableOperandRange PureCallOp::getOperandsMutable() {
  auto range = getODSOperandIndexAndLength(0);
  auto mutableRange = ::mlir::MutableOperandRange(getOperation(), range.first, range.second);
  return mutableRange;
}

::llvm::LogicalResult PureCallOp::setPropertiesFromAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }

  {
    auto &propStorage = prop.callee;
       auto attr = dict.get("callee");
    if (attr) {
      auto convertedAttr = ::llvm::dyn_cast<std::remove_reference_t<decltype(propStorage)>>(attr);
      if (convertedAttr) {
        propStorage = convertedAttr;
      } else {
        emitError() << "Invalid attribute `callee` in property conversion: " << attr;
        return ::mlir::failure();
      }
    }
  }
  return ::mlir::success();
}

::mlir::Attribute PureCallOp::getPropertiesAsAttr(::mlir::MLIRContext *ctx, const Properties &prop) {
    ::mlir::SmallVector<::mlir::NamedAttribute> attrs;
    ::mlir::Builder odsBuilder{ctx};

    {
      const auto &propStorage = prop.callee;
      if (propStorage)
        attrs.push_back(odsBuilder.getNamedAttr("callee",
                                       propStorage));
    }

  if (!attrs.empty())
    return odsBuilder.getDictionaryAttr(attrs);
  return {};
}

llvm::hash_code PureCallOp::computePropertiesHash(const Properties &prop) {
  using llvm::hash_value;
  return llvm::hash_combine(
    llvm::hash_value(prop.callee.getAsOpaquePointer()));
}

std::optional<mlir::Attribute> PureCallOp::getInherentAttr(::mlir::MLIRContext *ctx, const Properties &prop, llvm::StringRef name) {
    if (name == "callee")
      return prop.callee;
  return std::nullopt;
}

void PureCallOp::setInherentAttr(Properties &prop, llvm::StringRef name, mlir::Attribute value) {
    if (name == "callee") {
       prop.callee = ::llvm::dyn_cast_or_null<std::remove_reference_t<decltype(prop.callee)>>(value);
       return;
    }
}

void PureCallOp::populateInherentAttrs(::mlir::MLIRContext *ctx, const Properties &prop, ::mlir::NamedAttrList &attrs) {
    if (prop.callee) attrs.append("callee", prop.callee);
}

::llvm::LogicalResult PureCallOp::verifyInherentAttrs(::mlir::OperationName opName, ::mlir::NamedAttrList &attrs, llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
    {
      ::mlir::Attribute attr = attrs.get(getCalleeAttrName(opName));
      if (attr && ::mlir::failed(__mlir_ods_local_attr_constraint_AlkylOps3(attr, "callee", emitError)))
        return ::mlir::failure();
    }
    return ::mlir::success();
}

::llvm::LogicalResult PureCallOp::readProperties(::mlir::DialectBytecodeReader &reader, ::mlir::OperationState &state) {
  auto &prop = state.getOrAddProperties<Properties>(); (void)prop;
  if (::mlir::failed(reader.readAttribute(prop.callee)))
    return ::mlir::failure();
  return ::mlir::success();
}

void PureCallOp::writeProperties(::mlir::DialectBytecodeWriter &writer) {
  auto &prop = getProperties(); (void)prop;
  writer.writeAttribute(prop.callee);
}

::mlir::SymbolRefAttr PureCallOp::getCallee() {
  auto attr = getCalleeAttr();
  return attr;
}

void PureCallOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::Type resultType0, ::mlir::SymbolRefAttr callee, ::mlir::ValueRange operands) {
  odsState.addOperands(operands);
  odsState.getOrAddProperties<Properties>().callee = callee;
  odsState.addTypes(resultType0);
}

PureCallOp PureCallOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::Type resultType0, ::mlir::SymbolRefAttr callee, ::mlir::ValueRange operands) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultType0)>(resultType0), std::forward<decltype(callee)>(callee), std::forward<decltype(operands)>(operands));
  auto __res__ = ::llvm::dyn_cast<PureCallOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

PureCallOp PureCallOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::Type resultType0, ::mlir::SymbolRefAttr callee, ::mlir::ValueRange operands) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultType0)>(resultType0), std::forward<decltype(callee)>(callee), std::forward<decltype(operands)>(operands));
}

void PureCallOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::SymbolRefAttr callee, ::mlir::ValueRange operands) {
  odsState.addOperands(operands);
  odsState.getOrAddProperties<Properties>().callee = callee;
  assert(resultTypes.size() == 1u && "mismatched number of results");
  odsState.addTypes(resultTypes);
}

PureCallOp PureCallOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::SymbolRefAttr callee, ::mlir::ValueRange operands) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(callee)>(callee), std::forward<decltype(operands)>(operands));
  auto __res__ = ::llvm::dyn_cast<PureCallOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

PureCallOp PureCallOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::SymbolRefAttr callee, ::mlir::ValueRange operands) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(callee)>(callee), std::forward<decltype(operands)>(operands));
}

void PureCallOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  odsState.addOperands(operands);
  odsState.addAttributes(attributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);

  if (!attributes.empty()) {
    ::mlir::OpaqueProperties properties =
      &odsState.getOrAddProperties<PureCallOp::Properties>();
    std::optional<::mlir::RegisteredOperationName> info =
      odsState.name.getRegisteredInfo();
    if (failed(info->setOpPropertiesFromAttribute(odsState.name, properties,
        odsState.attributes.getDictionary(odsState.getContext()), nullptr)))
      ::llvm::report_fatal_error("Property conversion failed.");
  }
}

PureCallOp PureCallOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
  auto __res__ = ::llvm::dyn_cast<PureCallOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

PureCallOp PureCallOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
}

void PureCallOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  odsState.addOperands(operands);
  odsState.useProperties(const_cast<Properties&>(properties));
  odsState.addAttributes(discardableAttributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);
}

PureCallOp PureCallOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
  auto __res__ = ::llvm::dyn_cast<PureCallOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

PureCallOp PureCallOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
}

::llvm::LogicalResult PureCallOp::verifyInvariantsImpl() {
  auto tblgen_callee = getProperties().callee; (void)tblgen_callee;
  if (!tblgen_callee) return emitOpError("requires attribute 'callee'");

  if (::mlir::failed(__mlir_ods_local_attr_constraint_AlkylOps3(*this, tblgen_callee, "callee")))
    return ::mlir::failure();
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSOperands(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps3(*this, v.getType(), "operand", index++)))
        return ::mlir::failure();
    }
  }
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSResults(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps1(*this, v.getType(), "result", index++)))
        return ::mlir::failure();
    }
  }
  return ::mlir::success();
}

::llvm::LogicalResult PureCallOp::verifyInvariants() {
  return verifyInvariantsImpl();
}

::llvm::LogicalResult PureCallOp::setPropertiesFromParsedAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }
  // keep track of used keys in the input dictionary to be able to error out
  // if there are some unknown ones.
  ::mlir::DenseSet<::mlir::StringAttr> usedKeys;
  ::mlir::MLIRContext *ctx = dict.getContext();
  (void)ctx;
  {

    auto &propStorage = prop.callee;
    auto calleeAttrName = ::mlir::StringAttr::get(ctx, "callee");
    auto attr = dict.get(calleeAttrName);
    usedKeys.insert(calleeAttrName);
    if (attr || /*isRequired=*/true) {
      if (!attr) {
        emitError() << "expected key entry for callee in DictionaryAttr to set "
                   "Properties.";
        return ::mlir::failure();
      }
      auto convertedAttr = ::llvm::dyn_cast<std::remove_reference_t<decltype(propStorage)>>(attr);
      if (convertedAttr) {
        propStorage = convertedAttr;
      } else {
        emitError() << "Invalid attribute `callee` in property conversion: " << attr;
        return ::mlir::failure();
      }
    }
  }

  for (::mlir::NamedAttribute attr : dict) {
    if (!usedKeys.contains(attr.getName()))
      return emitError() << "unknown key '" << attr.getName() <<
          "' when parsing properties dictionary";
  }
  return ::mlir::success();
}

void PureCallOp::getEffects(::llvm::SmallVectorImpl<::mlir::SideEffects::EffectInstance<::mlir::MemoryEffects::Effect>> &effects) {
}


} // namespace alkyl_mlir
MLIR_DEFINE_EXPLICIT_TYPE_ID(::alkyl_mlir::PureCallOp)

namespace alkyl_mlir {


//===----------------------------------------------------------------------===//
// ::alkyl_mlir::TaintOp definitions
//===----------------------------------------------------------------------===//

namespace detail {

TaintOpGenericAdaptorBase::TaintOpGenericAdaptorBase(TaintOp op) : odsAttrs(op->getRawDictionaryAttrs()), odsOpName(op->getName()), properties(op.getProperties()), odsRegions(op->getRegions()) {}


} // namespace detail
TaintOpAdaptor::TaintOpAdaptor(TaintOp op) : TaintOpGenericAdaptor(op->getOperands(), op) {}

::llvm::LogicalResult TaintOpAdaptor::verify(::mlir::Location loc) {
  return ::mlir::success();
}

::llvm::LogicalResult TaintOp::setPropertiesFromAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }
      return ::mlir::success();
}

::mlir::Attribute TaintOp::getPropertiesAsAttr(::mlir::MLIRContext *ctx, const Properties &prop) {
    ::mlir::SmallVector<::mlir::NamedAttribute> attrs;
    ::mlir::Builder odsBuilder{ctx};

  if (!attrs.empty())
    return odsBuilder.getDictionaryAttr(attrs);
  return {};
}

llvm::hash_code TaintOp::computePropertiesHash(const Properties &prop) {
  using llvm::hash_value;
  return llvm::hash_combine();
}

std::optional<mlir::Attribute> TaintOp::getInherentAttr(::mlir::MLIRContext *ctx, const Properties &prop, llvm::StringRef name) {
  return std::nullopt;
}

void TaintOp::setInherentAttr(Properties &prop, llvm::StringRef name, mlir::Attribute value) {
}

void TaintOp::populateInherentAttrs(::mlir::MLIRContext *ctx, const Properties &prop, ::mlir::NamedAttrList &attrs) {
}

::llvm::LogicalResult TaintOp::verifyInherentAttrs(::mlir::OperationName opName, ::mlir::NamedAttrList &attrs, llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
    return ::mlir::success();
}

void TaintOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::Type tainted_val, ::mlir::Value pristine_val) {
  odsState.addOperands(pristine_val);
  odsState.addTypes(tainted_val);
}

TaintOp TaintOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::Type tainted_val, ::mlir::Value pristine_val) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(tainted_val)>(tainted_val), std::forward<decltype(pristine_val)>(pristine_val));
  auto __res__ = ::llvm::dyn_cast<TaintOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

TaintOp TaintOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::Type tainted_val, ::mlir::Value pristine_val) {
  return create(builder, builder.getLoc(), std::forward<decltype(tainted_val)>(tainted_val), std::forward<decltype(pristine_val)>(pristine_val));
}

void TaintOp::build(::mlir::OpBuilder &odsBuilder, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::Value pristine_val) {
  odsState.addOperands(pristine_val);
  assert(resultTypes.size() == 1u && "mismatched number of results");
  odsState.addTypes(resultTypes);
}

TaintOp TaintOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::Value pristine_val) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(pristine_val)>(pristine_val));
  auto __res__ = ::llvm::dyn_cast<TaintOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

TaintOp TaintOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::Value pristine_val) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(pristine_val)>(pristine_val));
}

void TaintOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  assert(operands.size() == 1u && "mismatched number of parameters");
  odsState.addOperands(operands);
  odsState.addAttributes(attributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);
}

TaintOp TaintOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
  auto __res__ = ::llvm::dyn_cast<TaintOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

TaintOp TaintOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, ::llvm::ArrayRef<::mlir::NamedAttribute> attributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(attributes)>(attributes));
}

void TaintOp::build(::mlir::OpBuilder &, ::mlir::OperationState &odsState, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  assert(operands.size() == 1u && "mismatched number of parameters");
  odsState.addOperands(operands);
  odsState.useProperties(const_cast<Properties&>(properties));
  odsState.addAttributes(discardableAttributes);
  assert(resultTypes.size() == 1u && "mismatched number of return types");
  odsState.addTypes(resultTypes);
}

TaintOp TaintOp::create(::mlir::OpBuilder &builder, ::mlir::Location location, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  ::mlir::OperationState __state__(location, getOperationName());
  build(builder, __state__, std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
  auto __res__ = ::llvm::dyn_cast<TaintOp>(builder.create(__state__));
  assert(__res__ && "builder didn't return the right type");
  return __res__;
}

TaintOp TaintOp::create(::mlir::ImplicitLocOpBuilder &builder, ::mlir::TypeRange resultTypes, ::mlir::ValueRange operands, const Properties &properties, ::llvm::ArrayRef<::mlir::NamedAttribute> discardableAttributes) {
  return create(builder, builder.getLoc(), std::forward<decltype(resultTypes)>(resultTypes), std::forward<decltype(operands)>(operands), std::forward<decltype(properties)>(properties), std::forward<decltype(discardableAttributes)>(discardableAttributes));
}

::llvm::LogicalResult TaintOp::verifyInvariantsImpl() {
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSOperands(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps1(*this, v.getType(), "operand", index++)))
        return ::mlir::failure();
    }
  }
  {
    unsigned index = 0; (void)index;
    auto valueGroup0 = getODSResults(0);

    for (auto v : valueGroup0) {
      if (::mlir::failed(__mlir_ods_local_type_constraint_AlkylOps1(*this, v.getType(), "result", index++)))
        return ::mlir::failure();
    }
  }
  return ::mlir::success();
}

::llvm::LogicalResult TaintOp::verifyInvariants() {
  return verifyInvariantsImpl();
}

::llvm::LogicalResult TaintOp::setPropertiesFromParsedAttr(Properties &prop, ::mlir::Attribute attr, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  ::mlir::DictionaryAttr dict = ::llvm::dyn_cast<::mlir::DictionaryAttr>(attr);
  if (!dict) {
    emitError() << "expected DictionaryAttr to set properties";
    return ::mlir::failure();
  }
  // keep track of used keys in the input dictionary to be able to error out
  // if there are some unknown ones.
  ::mlir::DenseSet<::mlir::StringAttr> usedKeys;
  ::mlir::MLIRContext *ctx = dict.getContext();
  (void)ctx;

  for (::mlir::NamedAttribute attr : dict) {
    if (!usedKeys.contains(attr.getName()))
      return emitError() << "unknown key '" << attr.getName() <<
          "' when parsing properties dictionary";
  }
  return ::mlir::success();
}

void TaintOp::getEffects(::llvm::SmallVectorImpl<::mlir::SideEffects::EffectInstance<::mlir::MemoryEffects::Effect>> &effects) {
}


} // namespace alkyl_mlir
MLIR_DEFINE_EXPLICIT_TYPE_ID(::alkyl_mlir::TaintOp)


#endif // GET_OP_CLASSES

