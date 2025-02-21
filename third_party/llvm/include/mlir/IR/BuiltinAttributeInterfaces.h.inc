/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|* Interface Declarations                                                     *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

namespace mlir {
class ElementsAttr;
namespace detail {
struct ElementsAttrInterfaceTraits {
  struct Concept {
    ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> (*getValuesImpl)(const Concept *impl, ::mlir::Attribute , ::mlir::TypeID);
    bool (*isSplat)(const Concept *impl, ::mlir::Attribute );
    ::mlir::ShapedType (*getType)(const Concept *impl, ::mlir::Attribute );
  };
  template<typename ConcreteAttr>
  class Model : public Concept {
  public:
    using Interface = ::mlir::ElementsAttr;
    Model() : Concept{getValuesImpl, isSplat, getType} {}

    static inline ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> getValuesImpl(const Concept *impl, ::mlir::Attribute tablegen_opaque_val, ::mlir::TypeID elementID);
    static inline bool isSplat(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
    static inline ::mlir::ShapedType getType(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
  };
  template<typename ConcreteAttr>
  class FallbackModel : public Concept {
  public:
    using Interface = ::mlir::ElementsAttr;
    FallbackModel() : Concept{getValuesImpl, isSplat, getType} {}

    static inline ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> getValuesImpl(const Concept *impl, ::mlir::Attribute tablegen_opaque_val, ::mlir::TypeID elementID);
    static inline bool isSplat(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
    static inline ::mlir::ShapedType getType(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
  };
  template<typename ConcreteModel, typename ConcreteAttr>
  class ExternalModel : public FallbackModel<ConcreteModel> {
  public:
    using ConcreteEntity = ConcreteAttr;
    ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> getValuesImpl(::mlir::Attribute tablegen_opaque_val, ::mlir::TypeID elementID) const;
    bool isSplat(::mlir::Attribute tablegen_opaque_val) const;
  };
};template <typename ConcreteAttr>
struct ElementsAttrTrait;

} // namespace detail
class ElementsAttr : public ::mlir::AttributeInterface<ElementsAttr, detail::ElementsAttrInterfaceTraits> {
public:
  using ::mlir::AttributeInterface<ElementsAttr, detail::ElementsAttrInterfaceTraits>::AttributeInterface;
  template <typename ConcreteAttr>
  struct Trait : public detail::ElementsAttrTrait<ConcreteAttr> {};
  /// This method returns an opaque range indexer for the given elementID, which
  /// corresponds to a desired C++ element data type. Returns the indexer if the
  /// attribute supports the given data type, failure otherwise.
  ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> getValuesImpl(::mlir::TypeID elementID) const;
  /// Returns true if the attribute elements correspond to a splat, i.e. that
  /// all elements of the attribute are the same value.
  bool isSplat() const;
  /// Returns the shaped type of the elements attribute.
  ::mlir::ShapedType getType() const;

    template <typename T>
    using iterator = detail::ElementsAttrIterator<T>;
    template <typename T>
    using iterator_range = detail::ElementsAttrRange<iterator<T>>;

    //===------------------------------------------------------------------===//
    // Accessors
    //===------------------------------------------------------------------===//

    /// Return the element type of this ElementsAttr.
    Type getElementType() const { return getElementType(*this); }
    static Type getElementType(ElementsAttr elementsAttr);

    /// Return if the given 'index' refers to a valid element in this attribute.
    bool isValidIndex(ArrayRef<uint64_t> index) const {
      return isValidIndex(*this, index);
    }
    static bool isValidIndex(ShapedType type, ArrayRef<uint64_t> index);
    static bool isValidIndex(ElementsAttr elementsAttr,
                             ArrayRef<uint64_t> index);

    /// Return the 1 dimensional flattened row-major index from the given
    /// multi-dimensional index.
    uint64_t getFlattenedIndex(ArrayRef<uint64_t> index) const {
      return getFlattenedIndex(*this, index);
    }
    static uint64_t getFlattenedIndex(Type type,
                                      ArrayRef<uint64_t> index);
    static uint64_t getFlattenedIndex(ElementsAttr elementsAttr,
                                      ArrayRef<uint64_t> index) {
      return getFlattenedIndex(elementsAttr.getType(), index);
    }

    /// Returns the number of elements held by this attribute.
    int64_t getNumElements() const { return getNumElements(*this); }
    static int64_t getNumElements(ElementsAttr elementsAttr);

    //===------------------------------------------------------------------===//
    // Value Iteration
    //===------------------------------------------------------------------===//

    template <typename T>
    using DerivedAttrValueCheckT =
        std::enable_if_t<std::is_base_of<Attribute, T>::value &&
                         !std::is_same<Attribute, T>::value>;
    template <typename T, typename ResultT>
    using DefaultValueCheckT =
        std::enable_if_t<std::is_same<Attribute, T>::value ||
                             !std::is_base_of<Attribute, T>::value,
                         ResultT>;

    /// Return the splat value for this attribute. This asserts that the
    /// attribute corresponds to a splat.
    template <typename T>
    T getSplatValue() const {
      assert(isSplat() && "expected splat attribute");
      return *value_begin<T>();
    }

    /// Return the elements of this attribute as a value of type 'T'.
    template <typename T>
    DefaultValueCheckT<T, iterator_range<T>> getValues() const {
      return {getType(), value_begin<T>(), value_end<T>()};
    }
    template <typename T>
    DefaultValueCheckT<T, iterator<T>> value_begin() const;
    template <typename T>
    DefaultValueCheckT<T, iterator<T>> value_end() const {
      return iterator<T>({}, size());
    }

    /// Return the held element values a range of T, where T is a derived
    /// attribute type.
    template <typename T>
    using DerivedAttrValueIterator =
      llvm::mapped_iterator<iterator<Attribute>, T (*)(Attribute)>;
    template <typename T>
    using DerivedAttrValueIteratorRange =
      detail::ElementsAttrRange<DerivedAttrValueIterator<T>>;
    template <typename T, typename = DerivedAttrValueCheckT<T>>
    DerivedAttrValueIteratorRange<T> getValues() const {
      auto castFn = [](Attribute attr) { return attr.template cast<T>(); };
      return {getType(), llvm::map_range(getValues<Attribute>(),
              static_cast<T (*)(Attribute)>(castFn))};
    }
    template <typename T, typename = DerivedAttrValueCheckT<T>>
    DerivedAttrValueIterator<T> value_begin() const {
      return getValues<T>().begin();
    }
    template <typename T, typename = DerivedAttrValueCheckT<T>>
    DerivedAttrValueIterator<T> value_end() const {
      return {value_end<Attribute>(), nullptr};
    }

    //===------------------------------------------------------------------===//
    // Failable Value Iteration

    /// If this attribute supports iterating over element values of type `T`,
    /// return the iterable range. Otherwise, return std::nullopt.
    template <typename T>
    DefaultValueCheckT<T, Optional<iterator_range<T>>> tryGetValues() const {
      if (Optional<iterator<T>> beginIt = try_value_begin<T>())
        return iterator_range<T>(getType(), *beginIt, value_end<T>());
      return std::nullopt;
    }
    template <typename T>
    DefaultValueCheckT<T, Optional<iterator<T>>> try_value_begin() const;

    /// If this attribute supports iterating over element values of type `T`,
    /// return the iterable range. Otherwise, return std::nullopt.
    template <typename T, typename = DerivedAttrValueCheckT<T>>
    Optional<DerivedAttrValueIteratorRange<T>> tryGetValues() const {
      auto values = tryGetValues<Attribute>();
      if (!values)
        return std::nullopt;

      auto castFn = [](Attribute attr) { return attr.template cast<T>(); };
      return DerivedAttrValueIteratorRange<T>(
        getType(),
        llvm::map_range(*values, static_cast<T (*)(Attribute)>(castFn))
      );
    }
    template <typename T, typename = DerivedAttrValueCheckT<T>>
    Optional<DerivedAttrValueIterator<T>> try_value_begin() const {
      if (auto values = tryGetValues<T>())
        return values->begin();
      return std::nullopt;
    }
  
    /// Return the number of elements held by this attribute.
    int64_t size() const { return getNumElements(); }

    /// Return if the attribute holds no elements.
    bool empty() const { return size() == 0; }
  
};
namespace detail {
  template <typename ConcreteAttr>
  struct ElementsAttrTrait : public ::mlir::AttributeInterface<ElementsAttr, detail::ElementsAttrInterfaceTraits>::Trait<ConcreteAttr> {
    /// This method returns an opaque range indexer for the given elementID, which
    /// corresponds to a desired C++ element data type. Returns the indexer if the
    /// attribute supports the given data type, failure otherwise.
    ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> getValuesImpl(::mlir::TypeID elementID) const {
      auto result = getValueImpl(
        (typename ConcreteAttr::ContiguousIterableTypesT *)nullptr, elementID,
        /*isContiguous=*/std::true_type());
      if (succeeded(result))
        return std::move(result);

      return getValueImpl(
        (typename ConcreteAttr::NonContiguousIterableTypesT *)nullptr,
        elementID, /*isContiguous=*/std::false_type());
    }
    /// Returns true if the attribute elements correspond to a splat, i.e. that
    /// all elements of the attribute are the same value.
    bool isSplat() const {
      // By default, only check for a single element splat.
        return (*static_cast<const ConcreteAttr *>(this)).getNumElements() == 1;
    }

    // By default, no types are iterable.
    using ContiguousIterableTypesT = std::tuple<>;
    using NonContiguousIterableTypesT = std::tuple<>;

    //===------------------------------------------------------------------===//
    // Accessors
    //===------------------------------------------------------------------===//

    /// Return the element type of this ElementsAttr.
    Type getElementType() const {
      return ::mlir::ElementsAttr::getElementType((*static_cast<const ConcreteAttr *>(this)));
    }

    /// Returns the number of elements held by this attribute.
    int64_t getNumElements() const {
      return ::mlir::ElementsAttr::getNumElements((*static_cast<const ConcreteAttr *>(this)));
    }

    /// Return if the given 'index' refers to a valid element in this attribute.
    bool isValidIndex(ArrayRef<uint64_t> index) const {
      return ::mlir::ElementsAttr::isValidIndex((*static_cast<const ConcreteAttr *>(this)), index);
    }

  protected:
    /// Returns the 1-dimensional flattened row-major index from the given
    /// multi-dimensional index.
    uint64_t getFlattenedIndex(ArrayRef<uint64_t> index) const {
      return ::mlir::ElementsAttr::getFlattenedIndex((*static_cast<const ConcreteAttr *>(this)), index);
    }

    //===------------------------------------------------------------------===//
    // Value Iteration Internals
    //===------------------------------------------------------------------===//
  protected:
    /// This class is used to allow specifying function overloads for different
    /// types, without actually taking the types as parameters. This avoids the
    /// need to build complicated SFINAE to select specific overloads.
    template <typename T>
    struct OverloadToken {};

  private:
    /// This function unpacks the types within a given tuple and then forwards
    /// on to the unwrapped variant.
    template <typename... Ts, typename IsContiguousT>
    auto getValueImpl(std::tuple<Ts...> *, ::mlir::TypeID elementID,
                      IsContiguousT isContiguous) const {
      return getValueImpl<Ts...>(elementID, isContiguous);
    }
    /// Check to see if the given `elementID` matches the current type `T`. If
    /// it does, build a value result using the current type. If it doesn't,
    /// keep looking for the desired type.
    template <typename T, typename... Ts, typename IsContiguousT>
    auto getValueImpl(::mlir::TypeID elementID,
                      IsContiguousT isContiguous) const {
      if (::mlir::TypeID::get<T>() == elementID)
        return buildValueResult<T>(isContiguous);
      return getValueImpl<Ts...>(elementID, isContiguous);
    }
    /// Bottom out case for no matching type.
    template <typename IsContiguousT>
    ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer>
    getValueImpl(::mlir::TypeID, IsContiguousT) const {
      return failure();
    }

    /// Build an indexer for the given type `T`, which is represented via a
    /// contiguous range.
    template <typename T>
    ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> buildValueResult(
        /*isContiguous*/std::true_type) const {
      if ((*static_cast<const ConcreteAttr *>(this)).empty()) {
        return ::mlir::detail::ElementsAttrIndexer::contiguous<T>(
          /*isSplat=*/false, nullptr);
      }

      auto valueIt = (*static_cast<const ConcreteAttr *>(this)).try_value_begin_impl(OverloadToken<T>());
      if (::mlir::failed(valueIt))
        return ::mlir::failure();
      return ::mlir::detail::ElementsAttrIndexer::contiguous(
        (*static_cast<const ConcreteAttr *>(this)).isSplat(), &**valueIt);
    }
    /// Build an indexer for the given type `T`, which is represented via a
    /// non-contiguous range.
    template <typename T>
    ::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> buildValueResult(
        /*isContiguous*/std::false_type) const {
      auto valueIt = (*static_cast<const ConcreteAttr *>(this)).try_value_begin_impl(OverloadToken<T>());
      if (::mlir::failed(valueIt))
        return ::mlir::failure();
      return ::mlir::detail::ElementsAttrIndexer::nonContiguous(
        (*static_cast<const ConcreteAttr *>(this)).isSplat(), *valueIt);
    }

  public:
    //===------------------------------------------------------------------===//
    // Value Iteration
    //===------------------------------------------------------------------===//

    /// The iterator for the given element type T.
    template <typename T, typename AttrT = ConcreteAttr>
    using iterator = decltype(std::declval<AttrT>().template value_begin<T>());
    /// The iterator range over the given element T.
    template <typename T, typename AttrT = ConcreteAttr>
    using iterator_range =
        decltype(std::declval<AttrT>().template getValues<T>());

    /// Return an iterator to the first element of this attribute as a value of
    /// type `T`.
    template <typename T>
    auto value_begin() const {
      return *(*static_cast<const ConcreteAttr *>(this)).try_value_begin_impl(OverloadToken<T>());
    }

    /// Return the elements of this attribute as a value of type 'T'.
    template <typename T>
    auto getValues() const {
      auto beginIt = (*static_cast<const ConcreteAttr *>(this)).template value_begin<T>();
      return detail::ElementsAttrRange<decltype(beginIt)>(
        (*static_cast<const ConcreteAttr *>(this)).getType(), beginIt, std::next(beginIt, size()));
    }
  
    /// Return the number of elements held by this attribute.
    int64_t size() const { return getNumElements(); }

    /// Return if the attribute holds no elements.
    bool empty() const { return size() == 0; }
  
  };
}// namespace detail
} // namespace mlir
namespace mlir {
class MemRefLayoutAttrInterface;
namespace detail {
struct MemRefLayoutAttrInterfaceInterfaceTraits {
  struct Concept {
    ::mlir::AffineMap (*getAffineMap)(const Concept *impl, ::mlir::Attribute );
    bool (*isIdentity)(const Concept *impl, ::mlir::Attribute );
    ::mlir::LogicalResult (*verifyLayout)(const Concept *impl, ::mlir::Attribute , ::llvm::ArrayRef<int64_t>, ::llvm::function_ref<::mlir::InFlightDiagnostic()>);
  };
  template<typename ConcreteAttr>
  class Model : public Concept {
  public:
    using Interface = ::mlir::MemRefLayoutAttrInterface;
    Model() : Concept{getAffineMap, isIdentity, verifyLayout} {}

    static inline ::mlir::AffineMap getAffineMap(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
    static inline bool isIdentity(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
    static inline ::mlir::LogicalResult verifyLayout(const Concept *impl, ::mlir::Attribute tablegen_opaque_val, ::llvm::ArrayRef<int64_t> shape, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError);
  };
  template<typename ConcreteAttr>
  class FallbackModel : public Concept {
  public:
    using Interface = ::mlir::MemRefLayoutAttrInterface;
    FallbackModel() : Concept{getAffineMap, isIdentity, verifyLayout} {}

    static inline ::mlir::AffineMap getAffineMap(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
    static inline bool isIdentity(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
    static inline ::mlir::LogicalResult verifyLayout(const Concept *impl, ::mlir::Attribute tablegen_opaque_val, ::llvm::ArrayRef<int64_t> shape, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError);
  };
  template<typename ConcreteModel, typename ConcreteAttr>
  class ExternalModel : public FallbackModel<ConcreteModel> {
  public:
    using ConcreteEntity = ConcreteAttr;
    bool isIdentity(::mlir::Attribute tablegen_opaque_val) const;
    ::mlir::LogicalResult verifyLayout(::mlir::Attribute tablegen_opaque_val, ::llvm::ArrayRef<int64_t> shape, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const;
  };
};template <typename ConcreteAttr>
struct MemRefLayoutAttrInterfaceTrait;

} // namespace detail
class MemRefLayoutAttrInterface : public ::mlir::AttributeInterface<MemRefLayoutAttrInterface, detail::MemRefLayoutAttrInterfaceInterfaceTraits> {
public:
  using ::mlir::AttributeInterface<MemRefLayoutAttrInterface, detail::MemRefLayoutAttrInterfaceInterfaceTraits>::AttributeInterface;
  template <typename ConcreteAttr>
  struct Trait : public detail::MemRefLayoutAttrInterfaceTrait<ConcreteAttr> {};
  /// Get the MemRef layout as an AffineMap, the method must not return NULL
  ::mlir::AffineMap getAffineMap() const;
  /// Return true if this attribute represents the identity layout
  bool isIdentity() const;
  /// Check if the current layout is applicable to the provided shape
  ::mlir::LogicalResult verifyLayout(::llvm::ArrayRef<int64_t> shape, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const;
};
namespace detail {
  template <typename ConcreteAttr>
  struct MemRefLayoutAttrInterfaceTrait : public ::mlir::AttributeInterface<MemRefLayoutAttrInterface, detail::MemRefLayoutAttrInterfaceInterfaceTraits>::Trait<ConcreteAttr> {
    /// Return true if this attribute represents the identity layout
    bool isIdentity() const {
      return (*static_cast<const ConcreteAttr *>(this)).getAffineMap().isIdentity();
    }
    /// Check if the current layout is applicable to the provided shape
    ::mlir::LogicalResult verifyLayout(::llvm::ArrayRef<int64_t> shape, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const {
      return ::mlir::detail::verifyAffineMapAsLayout((*static_cast<const ConcreteAttr *>(this)).getAffineMap(),
                                                       shape, emitError);
    }
  };
}// namespace detail
} // namespace mlir
namespace mlir {
class TypedAttr;
namespace detail {
struct TypedAttrInterfaceTraits {
  struct Concept {
    ::mlir::Type (*getType)(const Concept *impl, ::mlir::Attribute );
  };
  template<typename ConcreteAttr>
  class Model : public Concept {
  public:
    using Interface = ::mlir::TypedAttr;
    Model() : Concept{getType} {}

    static inline ::mlir::Type getType(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
  };
  template<typename ConcreteAttr>
  class FallbackModel : public Concept {
  public:
    using Interface = ::mlir::TypedAttr;
    FallbackModel() : Concept{getType} {}

    static inline ::mlir::Type getType(const Concept *impl, ::mlir::Attribute tablegen_opaque_val);
  };
  template<typename ConcreteModel, typename ConcreteAttr>
  class ExternalModel : public FallbackModel<ConcreteModel> {
  public:
    using ConcreteEntity = ConcreteAttr;
  };
};template <typename ConcreteAttr>
struct TypedAttrTrait;

} // namespace detail
class TypedAttr : public ::mlir::AttributeInterface<TypedAttr, detail::TypedAttrInterfaceTraits> {
public:
  using ::mlir::AttributeInterface<TypedAttr, detail::TypedAttrInterfaceTraits>::AttributeInterface;
  template <typename ConcreteAttr>
  struct Trait : public detail::TypedAttrTrait<ConcreteAttr> {};
  /// Get the attribute's type
  ::mlir::Type getType() const;
};
namespace detail {
  template <typename ConcreteAttr>
  struct TypedAttrTrait : public ::mlir::AttributeInterface<TypedAttr, detail::TypedAttrInterfaceTraits>::Trait<ConcreteAttr> {
  };
}// namespace detail
} // namespace mlir
namespace mlir {
template<typename ConcreteAttr>
::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> detail::ElementsAttrInterfaceTraits::Model<ConcreteAttr>::getValuesImpl(const Concept *impl, ::mlir::Attribute tablegen_opaque_val, ::mlir::TypeID elementID) {
  return (tablegen_opaque_val.cast<ConcreteAttr>()).getValuesImpl(elementID);
}
template<typename ConcreteAttr>
bool detail::ElementsAttrInterfaceTraits::Model<ConcreteAttr>::isSplat(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return (tablegen_opaque_val.cast<ConcreteAttr>()).isSplat();
}
template<typename ConcreteAttr>
::mlir::ShapedType detail::ElementsAttrInterfaceTraits::Model<ConcreteAttr>::getType(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return (tablegen_opaque_val.cast<ConcreteAttr>()).getType();
}
template<typename ConcreteAttr>
::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> detail::ElementsAttrInterfaceTraits::FallbackModel<ConcreteAttr>::getValuesImpl(const Concept *impl, ::mlir::Attribute tablegen_opaque_val, ::mlir::TypeID elementID) {
  return static_cast<const ConcreteAttr *>(impl)->getValuesImpl(tablegen_opaque_val, elementID);
}
template<typename ConcreteAttr>
bool detail::ElementsAttrInterfaceTraits::FallbackModel<ConcreteAttr>::isSplat(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return static_cast<const ConcreteAttr *>(impl)->isSplat(tablegen_opaque_val);
}
template<typename ConcreteAttr>
::mlir::ShapedType detail::ElementsAttrInterfaceTraits::FallbackModel<ConcreteAttr>::getType(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return static_cast<const ConcreteAttr *>(impl)->getType(tablegen_opaque_val);
}
template<typename ConcreteModel, typename ConcreteAttr>
::mlir::FailureOr<::mlir::detail::ElementsAttrIndexer> detail::ElementsAttrInterfaceTraits::ExternalModel<ConcreteModel, ConcreteAttr>::getValuesImpl(::mlir::Attribute tablegen_opaque_val, ::mlir::TypeID elementID) const {
auto result = getValueImpl(
        (typename ConcreteAttr::ContiguousIterableTypesT *)nullptr, elementID,
        /*isContiguous=*/std::true_type());
      if (succeeded(result))
        return std::move(result);

      return getValueImpl(
        (typename ConcreteAttr::NonContiguousIterableTypesT *)nullptr,
        elementID, /*isContiguous=*/std::false_type());
}
template<typename ConcreteModel, typename ConcreteAttr>
bool detail::ElementsAttrInterfaceTraits::ExternalModel<ConcreteModel, ConcreteAttr>::isSplat(::mlir::Attribute tablegen_opaque_val) const {
// By default, only check for a single element splat.
        return (tablegen_opaque_val.cast<ConcreteAttr>()).getNumElements() == 1;
}
} // namespace mlir
namespace mlir {
template<typename ConcreteAttr>
::mlir::AffineMap detail::MemRefLayoutAttrInterfaceInterfaceTraits::Model<ConcreteAttr>::getAffineMap(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return (tablegen_opaque_val.cast<ConcreteAttr>()).getAffineMap();
}
template<typename ConcreteAttr>
bool detail::MemRefLayoutAttrInterfaceInterfaceTraits::Model<ConcreteAttr>::isIdentity(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return (tablegen_opaque_val.cast<ConcreteAttr>()).isIdentity();
}
template<typename ConcreteAttr>
::mlir::LogicalResult detail::MemRefLayoutAttrInterfaceInterfaceTraits::Model<ConcreteAttr>::verifyLayout(const Concept *impl, ::mlir::Attribute tablegen_opaque_val, ::llvm::ArrayRef<int64_t> shape, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  return (tablegen_opaque_val.cast<ConcreteAttr>()).verifyLayout(shape, emitError);
}
template<typename ConcreteAttr>
::mlir::AffineMap detail::MemRefLayoutAttrInterfaceInterfaceTraits::FallbackModel<ConcreteAttr>::getAffineMap(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return static_cast<const ConcreteAttr *>(impl)->getAffineMap(tablegen_opaque_val);
}
template<typename ConcreteAttr>
bool detail::MemRefLayoutAttrInterfaceInterfaceTraits::FallbackModel<ConcreteAttr>::isIdentity(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return static_cast<const ConcreteAttr *>(impl)->isIdentity(tablegen_opaque_val);
}
template<typename ConcreteAttr>
::mlir::LogicalResult detail::MemRefLayoutAttrInterfaceInterfaceTraits::FallbackModel<ConcreteAttr>::verifyLayout(const Concept *impl, ::mlir::Attribute tablegen_opaque_val, ::llvm::ArrayRef<int64_t> shape, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) {
  return static_cast<const ConcreteAttr *>(impl)->verifyLayout(tablegen_opaque_val, shape, emitError);
}
template<typename ConcreteModel, typename ConcreteAttr>
bool detail::MemRefLayoutAttrInterfaceInterfaceTraits::ExternalModel<ConcreteModel, ConcreteAttr>::isIdentity(::mlir::Attribute tablegen_opaque_val) const {
return (tablegen_opaque_val.cast<ConcreteAttr>()).getAffineMap().isIdentity();
}
template<typename ConcreteModel, typename ConcreteAttr>
::mlir::LogicalResult detail::MemRefLayoutAttrInterfaceInterfaceTraits::ExternalModel<ConcreteModel, ConcreteAttr>::verifyLayout(::mlir::Attribute tablegen_opaque_val, ::llvm::ArrayRef<int64_t> shape, ::llvm::function_ref<::mlir::InFlightDiagnostic()> emitError) const {
return ::mlir::detail::verifyAffineMapAsLayout((tablegen_opaque_val.cast<ConcreteAttr>()).getAffineMap(),
                                                       shape, emitError);
}
} // namespace mlir
namespace mlir {
template<typename ConcreteAttr>
::mlir::Type detail::TypedAttrInterfaceTraits::Model<ConcreteAttr>::getType(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return (tablegen_opaque_val.cast<ConcreteAttr>()).getType();
}
template<typename ConcreteAttr>
::mlir::Type detail::TypedAttrInterfaceTraits::FallbackModel<ConcreteAttr>::getType(const Concept *impl, ::mlir::Attribute tablegen_opaque_val) {
  return static_cast<const ConcreteAttr *>(impl)->getType(tablegen_opaque_val);
}
} // namespace mlir
