#pragma once
// Included below the FTZ module declaration; all SIMD operations are dependent.

namespace ftz::detail {
  using ::simd::mask_bits;
  template <class V> using ftz32_bridge = native::detail::fp32_bit_bridge<V>;
  template <class V> using ftz32_words = typename V::bits_type;
  template <class U> simd_nodiscard simd_inline bool ftz32_any(U mask) noexcept {
    return any(mask != U(0));
  }
  template <class U> simd_nodiscard simd_inline simd_const U ftz32_import_words(U bits) noexcept {
    auto magnitude = bits & U(0x7fffffffu);
    auto tiny = U(0x00800000u) > magnitude;
    return select(tiny, bits & U(0x80000000u), bits);
  }
  template <class V> simd_nodiscard simd_inline simd_const ftz32_words<V> ftz32_nonfinite(V v) noexcept {
    using U = ftz32_words<V>;
    return mask_bits<std::uint32_t>(U(ftz32_infinity) > (ftz32_bridge<V>::encode(v) & U(0x7fffffffu))) ^ U(0xffffffffu);
  }
  template <class V, class... X>
  simd_nodiscard simd_inline simd_const ftz32_words<V> ftz32_repair_mask(V result, X...) noexcept {
    using U = ftz32_words<V>;
    auto boundary_mask = mask_bits<std::uint32_t>((ftz32_bridge<V>::encode(result) & U(0x7fffffffu)) > U(0x00800000u)) ^ U(0xffffffffu);
    return boundary_mask;
  }
  template <auto Repair, class A, std::size_t... I>
  simd_nodiscard simd_inline simd_pure std::uint32_t ftz32_repair_lane(A const & inputs, std::size_t lane, std::index_sequence<I...>) noexcept {
    return Repair(inputs[I][lane]...);
  }
  // No lane extraction on the normal path. Only flagged lanes call the shared
  // scalar contract; the operation and backend are compile-time selections.
  template <auto Repair, class V, class... X>
  simd_nodiscard simd_inline simd_const V ftz32_repair(V result, ftz32_words<V> mask, X... operands) noexcept {
    if (!ftz32_any(mask)) return result;
    using B = ftz32_bridge<V>;
    using U = ftz32_words<V>;
    std::array<std::uint32_t, V::lanes> flags, output;
    std::array<std::array<std::uint32_t, V::lanes>, sizeof...(X)> inputs;
    mask.store(flags.data()); B::encode(result).store(output.data());
    std::size_t index = 0;
    (B::encode(operands).store(inputs[index++].data()), ...);
    for (std::size_t lane = 0; lane < V::lanes; ++lane)
      if (flags[lane] != 0)
        output[lane] = ftz32_repair_lane<Repair>(inputs, lane, std::index_sequence_for<X...>{});
    return B::decode(U::load(output.data()));
  }
  simd_nodiscard simd_inline simd_const std::uint32_t ftz32_scaleb_repair(
      std::uint32_t result, std::uint32_t value, std::uint32_t exponent) noexcept {
    // All NaN inputs have the raw instruction's quiet-NaN value semantics for
    // infinite exponents, without changing the supplied input words.
    if ((value & 0x7fffffffu) > 0x7f800000u &&
        (exponent & 0x7fffffffu) == 0x7f800000u)
      return (exponent & 0x80000000u) != 0u ? 0u : 0x7f800000u;
    unsigned int biased = (value >> 23) & 0xffu;
    if (biased == 0u || biased == 255u || (value & 0x007fffffu) != 0x007fffffu)
      return result;
    // Only this exact halfway value rounds from a tiny mathematical result to
    // minimum normal. Compare the floor interval without converting an arbitrary
    // exponent to an integer; native nonfinite results retain their raw behavior.
    float shift = ::ftz::detail::math::fp32_decode(exponent);
    float lower = -static_cast<float>(biased);
    return shift >= lower && shift < lower + 1.0f ?
      (value & 0x80000000u) | 0x00800000u : result;
  }
  template <bool Hardware, class M, class V>
  simd_nodiscard simd_inline simd_const V ftz32_scaled_result(
      M active, V result, V value, V exponent) noexcept {
    using B = ftz32_bridge<V>; using U = ftz32_words<V>;
    U active_bits = mask_bits<std::uint32_t>(active), bits = B::encode(result);
    if constexpr(!Hardware) {
    U tiny = mask_bits<std::uint32_t>(U(0x00800000u) > (bits & U(0x7fffffffu)));
    bits = bits & ((active_bits & tiny & U(0x007fffffu)) ^ U(0xffffffffu));
    result = B::decode(bits);
    }
    // No scalar extraction on the ordinary path. Native FTZ may discard the
    // rounding-up boundary above; other scaling and special values stay native.
    U value_bits = B::encode(value);
    U boundary = mask_bits<std::uint32_t>(U(0x00800000u) > (bits & U(0x7fffffffu))) &
      mask_bits<std::uint32_t>((value_bits & U(0x007fffffu)) == U(0x007fffffu));
    U nan_infinite = mask_bits<std::uint32_t>((value_bits & U(0x7fffffffu)) > U(0x7f800000u)) &
      mask_bits<std::uint32_t>((B::encode(exponent) & U(0x7fffffffu)) == U(0x7f800000u));
    return ftz32_repair<ftz32_scaleb_repair>(result,active_bits & (boundary | nan_infinite),
      result,value,exponent);
  }
  template <bool Hardware, class V> simd_nodiscard simd_inline simd_const V ftz32_vector_add(V a, V b) noexcept {
    V r = a + b;
    if constexpr(Hardware) return r;
    else return ftz32_repair<ftz32_add<Hardware>>(r, ftz32_repair_mask(r, a, b), a, b);
  }
  template <bool Hardware, class V> simd_nodiscard simd_inline simd_const V ftz32_vector_sub(V a, V b) noexcept {
    V r = a - b;
    if constexpr(Hardware) return r;
    else return ftz32_repair<ftz32_sub<Hardware>>(r, ftz32_repair_mask(r, a, b), a, b);
  }
  template <class V> simd_nodiscard simd_inline simd_const V ftz32_vector_mul(V a, V b) noexcept {
    V r = a * b; return ftz32_repair<ftz32_mul>(r, ftz32_repair_mask(r, a, b), a, b);
  }
  template <class V> simd_nodiscard simd_inline simd_const V ftz32_vector_fma(V a, V b, V c) noexcept {
    V r = fma(a, b, c); return ftz32_repair<ftz32_fma>(r, ftz32_repair_mask(r, a, b, c), a, b, c);
  }
  // Same normalized three-refinement policy-3 graph as policy.h.
  // Every mantissa operation stays normal. Integer exponent scaling handles
  // ordinary results; unusual scales and IEEE-like special values use the core.
  template <bool Hardware, class V> simd_nodiscard simd_inline simd_const V ftz32_vector_div(V a, V b) noexcept {
    using B = ftz32_bridge<V>; using U = ftz32_words<V>;
    U aw = B::encode(a), bw = B::encode(b);
    U aa = aw & U(0x7fffffffu), bb = bw & U(0x7fffffffu);
    U ma = U(0x3f800000u) | (aa & U(0x007fffffu));
    U mb = U(0x3f800000u) | (bb & U(0x007fffffu));
    V m = B::decode(mb), r = B::decode(U(0x7ef311c3u) - mb);
    V e = fma(-m, r, V(1)); r = fma(r, e, r);
    e = fma(-m, r, V(1)); r = fma(r, e, r);
    e = fma(-m, r, V(1)); r = fma(r, e, r);
    r = B::decode(ma) * r;
    U rw = B::encode(r);
    U exponent = rw.template right<23>() + aa.template right<23>() - bb.template right<23>();
    U normal = mask_bits<std::uint32_t>((exponent > U(0)) & (U(255) > exponent));
    U bits = ((aw ^ bw) & U(0x80000000u)) | exponent.template left<23>() | (rw & U(0x007fffffu));
    U exceptional = (normal ^ U(0xffffffffu)) | mask_bits<std::uint32_t>((aa == U(0)) | (bb == U(0))) |
                    ftz32_nonfinite(a) | ftz32_nonfinite(b);
    return ftz32_repair<ftz32_div<Hardware>>(B::decode(bits), exceptional, a, b);
  }
  template <class V> simd_nodiscard simd_inline simd_const V ftz32_vector_sqrt(V a) noexcept {
    using B = ftz32_bridge<V>; using U = ftz32_words<V>;
    U aw = B::encode(a), aa = aw & U(0x7fffffffu), exponent_a = aa.template right<23>();
    U parity = U(1) - (exponent_a & U(1));
    U mword = (U(0x3f800000u) | (aa & U(0x007fffffu))) + parity.template left<23>();
    V m = B::decode(mword), r = B::decode(U(0x5f375a86u) - mword.template right<1>());
    V p = m * r, e = fma(-p, r, V(1)), h = V(0.5f) * r; r = fma(h, e, r);
    p = m * r; e = fma(-p, r, V(1)); h = V(0.5f) * r; r = fma(h, e, r);
    p = m * r; e = fma(-p, r, V(1)); h = V(0.5f) * r; r = fma(h, e, r);
    r = m * r;
    U rw = B::encode(r);
    U exponent = rw.template right<23>() + (exponent_a + U(127) - parity).template right<1>() - U(127);
    U bits = exponent.template left<23>() | (rw & U(0x007fffffu));
    U exceptional = mask_bits<std::uint32_t>((aa == U(0)) | ((aw & U(0x80000000u)) > U(0))) | ftz32_nonfinite(a);
    return ftz32_repair<ftz32_sqrt>(B::decode(bits), exceptional, a);
  }
}

namespace ftz::detail {
  template <class V> concept raw_register = requires { typename V::value_type; typename V::bits_type; V::lanes; } && std::same_as<typename V::value_type,float>;
  template <class R> concept ftz32_vector = requires { typename R::value_type; typename R::register_type; } && ftz32_type<typename R::value_type>;
  template <class V, class F = ftz32> using ftz32_simd = typename V::template rebind<F>;
}

/** \defgroup ftz_vectors FTZ SIMD values
 * Import `ftz` and the selected SIMD architecture module, then use
 * `simd::vec<ftz::m32,N,Arch>` or `simd::vec<ftz::h32,N,Arch>`.
 * The element policy applies to every lane; hardware values require an admitted
 * flush environment on the calling thread. The raw register's layout, lane
 * count and mask representation are retained. No operation performs admission.
 * Typed FTZ memory and swizzles transport existing bits; float imports normalize
 * signed subnormals. Unsafe factories transfer this invariant to the caller.
 * The examples instantiate `F` as m32 and h32; `arch` is the selected SIMD tag.
 * \snippet vectors.cc vector_memory
 * \snippet vectors.cc vector_masks
 *
 * `simd::wide` lifts these operations through ADL. Its mathematical results keep
 * the wide shape and FTZ element policy; classification returns a wide of masks.
 * \snippet vectors.cc wide_math
 */
/** \defgroup ftz_register_arrays Register-array math
 * \ingroup ftz_vectors
 * Matching arrays retain their element type and extent, including extent zero.
 * Transcendental arrays accept FTZ scalars or FTZ vectors. Batched add/sub/mul
 * and fma accept FTZ vectors, issuing native operations before boundary repair.
 * These named overloads also supply the array path used by `simd::wide`.
 * \snippet vectors.cc array_math
 */
export namespace simd {
  /// \ingroup ftz_vectors
  /// \brief Stores either FTZ scalar policy in the architecture's raw float register.
  template <bool Hardware> struct simd_traits<::ftz::basic_ftz32<Hardware>> { using storage_type = float; };
  /// \ingroup ftz_vectors
  /// \brief Supplies FTZ arithmetic and typed memory to simd::vec for any supported architecture.
  template <bool Hardware, class V, class Self>
  struct simd_customization<::ftz::basic_ftz32<Hardware>,V,Self> {
    using simd = Self;
    using ftz32 = ::ftz::basic_ftz32<Hardware>;
    static constexpr bool hardware = Hardware;
    static constexpr std::size_t N = V::lanes;
    using register_type = V;
    using value_type = ftz32;
    using native_type = typename V::native_type;
    using bits_type = ::ftz::detail::ftz32_words<V>;
    using mask_type = typename V::mask_type;
    using mask = mask_type;
    using vector_mask_type = typename V::vector_mask_type;
    static constexpr std::size_t lanes = V::lanes;
    /// \brief Constructs positive zero in every lane.
    simd_inline simd_customization() noexcept : value_(0.0f) {}
    /// \brief Imports and broadcasts a float, replacing signed subnormals with signed zero.
    simd_inline simd_customization(float value) noexcept : simd_customization(V(value)) {}
    /// \brief Broadcasts an already canonical FTZ scalar without normalization.
    simd_inline simd_customization(ftz32 value) noexcept : value_(value.to_float()) {}
    /// \brief Imports a raw float register, normalizing signed subnormal lanes.
    simd_inline simd_customization(V value) noexcept
      : value_(import(value)) {}
    /// \brief Returns raw float lanes unchanged; subsequent raw arithmetic leaves the FTZ contract.
    simd_inline operator V() const noexcept { return value_; }
    /// \brief Imports native storage through the normalizing raw-register constructor.
    simd_inline simd_customization(native_type value) noexcept requires (N>1) : simd_customization(V(value)) {}
    /// \brief Returns native storage unchanged; subsequent native arithmetic leaves the FTZ contract.
    simd_inline operator native_type() const noexcept { return value_.value; }
    /// \brief Imports one value per lane; conversion exceptions determine noexcept.
    template <class... X> requires (sizeof...(X)==N && N>1) && (std::convertible_to<X,ftz32> && ...)
    simd_inline simd_customization(X... x) noexcept((noexcept(static_cast<float>(x)) && ...))
      : simd_customization(V(static_cast<float>(x)...)) {}
    /// \brief Loads exactly N float or same-policy FTZ elements.
    template <class T> requires (std::same_as<T,float> || std::same_as<T,ftz32>)
    simd_inline simd_customization(std::array<T,N> const & values) noexcept : value_(load_memory(values.data()).to_native()) {}

    /// \brief Returns the raw float register with every stored bit unchanged.
    simd_nodiscard simd_inline simd_pure V to_native() const noexcept { return value_; }
    /// \brief Returns each lane as an unsigned 32-bit word, without floating-point evaluation.
    simd_nodiscard simd_inline simd_pure bits_type to_bits() const noexcept { return bits(); }
    /// \brief Imports and broadcasts a float, normalizing signed subnormals.
    simd_nodiscard static simd_inline simd_const simd from_float(float value) noexcept { return value; }
    /// \brief Imports raw float lanes and normalizes signed subnormals.
    simd_nodiscard static simd_inline simd_const simd from_native(V value) noexcept { return value; }
    // Caller promises normal, signed zero, infinity or any NaN lanes.
    // No classification or normalization; this is an explicit invariant escape.
    /// \brief Wraps raw float lanes unchanged; the caller must supply canonical FTZ values.
    simd_nodiscard static simd_inline simd_const simd unsafe_from_float32(V value) noexcept {
      return {canonical{}, value};
    }
    /// \brief Wraps raw float lanes unchanged; the caller must supply canonical FTZ values.
    simd_nodiscard static simd_inline simd_const simd unsafe_from_float32(float value) noexcept {
      return {canonical{}, V(value)};
    }
    /// \brief Returns the stored lane words without classification or normalization.
    simd_nodiscard simd_inline simd_pure bits_type bits() const noexcept { return bridge::encode(value_); }
    /// \brief Imports float words, replacing signed subnormal words with signed zero.
    simd_nodiscard static simd_inline simd_const simd from_bits(bits_type value) noexcept {
      return simd(bridge::decode(value));
    }
    /// \brief Imports float words, replacing signed subnormal words with signed zero.
    simd_nodiscard static simd_inline simd_const simd from_bits(std::uint32_t value) noexcept {
      return from_bits(bits_type(value));
    }
    /// \brief Loads exactly N elements; float memory is normalized, same-policy FTZ memory is
    /// copied exactly.
    template <std::size_t Alignment = 1, class T> requires (std::same_as<T,float> || std::same_as<T,ftz32>)
    simd_nodiscard static simd_inline simd load_memory(T const * p) noexcept {
      if constexpr (std::same_as<T,float>) return simd(V::template load_memory<Alignment>(p));
      else {
        std::array<std::uint32_t,N> words;
        std::memcpy(words.data(),p,sizeof(words));
        return unsafe_from_float32(V::from_bits(bits_type::load(words.data())));
      }
    }
    /// \brief Stores exactly N float or same-policy FTZ elements, preserving stored bits.
    template <std::size_t Alignment = 1, class T> requires (std::same_as<T,float> || std::same_as<T,ftz32>)
    simd_inline void store_memory(T * p) const noexcept {
      if constexpr (std::same_as<T,float>) value_.template store_memory<Alignment>(p);
      else {
        std::array<std::uint32_t,N> words; bits().store(words.data());
        std::memcpy(p,words.data(),sizeof(words));
      }
    }
    // Legacy pointer spellings forward to the safe unaligned memory helpers.
    /// \brief Loads N unaligned float elements and normalizes signed subnormals.
    simd_nodiscard static simd_inline simd_pure simd load(float const * p) noexcept {
      return load_memory(p);
    }
    /// \brief Loads N unaligned float elements and normalizes signed subnormals.
    simd_nodiscard static simd_inline simd_pure simd loadu(float const * p) noexcept { return load_memory(p); }
    /// \brief Stores N unaligned float elements without normalization.
    simd_inline void store(float * p) const noexcept { store_memory(p); }
    /// \brief Stores N unaligned float elements without normalization.
    simd_inline void storeu(float * p) const noexcept { store_memory(p); }
    /// \brief Imports n float elements and fills remaining lanes; requires n <= lanes, and n
    /// == 0 does not access p.
    simd_nodiscard static simd_inline simd_pure simd load_partial(float const * p, std::size_t n, float fill = 0) noexcept {
      std::array<float, lanes> temporary; temporary.fill(fill);
      for (std::size_t i=0;i<n;++i) temporary[i]=p[i];
      return loadu(temporary.data());
    }
    /// \brief Stores the first n lanes; requires n <= lanes, and n == 0 does not access p.
    simd_inline void store_partial(float * p, std::size_t n) const noexcept {
      std::array<float, lanes> temporary; storeu(temporary.data());
      for (std::size_t i=0;i<n;++i) p[i]=temporary[i];
    }
    /// \brief Imports N float words and normalizes signed subnormal words.
    simd_nodiscard static simd_inline simd_pure simd load_bits(std::uint32_t const * p) noexcept {
      return from_bits(bits_type::load(p));
    }
    /// \brief Stores N exact lane words without floating-point evaluation.
    simd_inline void store_bits(std::uint32_t * p) const noexcept { bits().store(p); }
    /// \brief Imports n words and fills remaining lanes; requires n <= lanes, with no access
    /// to p when n == 0.
    simd_nodiscard static simd_inline simd_pure simd load_bits_partial(
        std::uint32_t const * p, std::size_t n, std::uint32_t fill = 0) noexcept {
      return from_bits(bits_type::load_partial(p, n, fill));
    }
    /// \brief Stores n exact lane words; requires n <= lanes, with no access to p when n == 0.
    simd_inline void store_bits_partial(std::uint32_t * p, std::size_t n) const noexcept {
      bits().store_partial(p, n);
    }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    simd_nodiscard friend simd_inline simd_const simd operator+(simd a, simd b) noexcept {
      return {canonical{}, ::ftz::detail::ftz32_vector_add<Hardware>(a.value_, b.value_)};
    }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    simd_nodiscard friend simd_inline simd_const simd operator-(simd a, simd b) noexcept {
      return {canonical{}, ::ftz::detail::ftz32_vector_sub<Hardware>(a.value_, b.value_)};
    }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    simd_nodiscard friend simd_inline simd_const simd operator*(simd a, simd b) noexcept {
      return {canonical{}, ::ftz::detail::ftz32_vector_mul(a.value_, b.value_)};
    }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    simd_nodiscard friend simd_inline simd_const simd operator/(simd a, simd b) noexcept {
      return {canonical{}, ::ftz::detail::ftz32_vector_div<Hardware>(a.value_, b.value_)};
    }
    /// \brief Assigns the corresponding FTZ arithmetic result and returns this vector by reference.
    simd_inline simd & operator+=(simd b) noexcept { return static_cast<Self &>(*this) = static_cast<Self &>(*this) + b; }
    /// \brief Assigns the corresponding FTZ arithmetic result and returns this vector by reference.
    simd_inline simd & operator-=(simd b) noexcept { return static_cast<Self &>(*this) = static_cast<Self &>(*this) - b; }
    /// \brief Assigns the corresponding FTZ arithmetic result and returns this vector by reference.
    simd_inline simd & operator*=(simd b) noexcept { return static_cast<Self &>(*this) = static_cast<Self &>(*this) * b; }
    /// \brief Assigns the corresponding FTZ arithmetic result and returns this vector by reference.
    simd_inline simd & operator/=(simd b) noexcept { return static_cast<Self &>(*this) = static_cast<Self &>(*this) / b; }
    /// \brief Flips lane sign bits exactly, including signed zero and NaN payloads.
    simd_nodiscard friend simd_inline simd_const simd operator-(simd a) noexcept {
      return {canonical{}, bridge::decode(a.bits() ^ bits_type(0x80000000u))};
    }
    /// \brief Returns the input vector unchanged.
    simd_nodiscard friend simd_inline simd_const simd operator+(simd a) noexcept { return a; }
    /// \brief Clears each sign bit, preserving magnitude words including NaN payloads.
    simd_nodiscard friend simd_inline simd_const simd abs(simd a) noexcept {
      return {canonical{}, bridge::decode(a.bits() & bits_type(0x7fffffffu))};
    }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    simd_nodiscard friend simd_inline simd_const mask_type operator<(simd a, simd b) noexcept { return a.value_ < b.value_; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    simd_nodiscard friend simd_inline simd_const mask_type operator>(simd a, simd b) noexcept { return a.value_ > b.value_; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    simd_nodiscard friend simd_inline simd_const mask_type operator==(simd a, simd b) noexcept { return a.value_ == b.value_; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    simd_nodiscard friend simd_inline simd_const mask_type operator!=(simd a, simd b) noexcept { return ~(a == b); }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    simd_nodiscard friend simd_inline simd_const mask_type operator<=(simd a, simd b) noexcept { return (a < b) | (a == b); }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    simd_nodiscard friend simd_inline simd_const mask_type operator>=(simd a, simd b) noexcept { return (a > b) | (a == b); }
    /// \brief Selects complete lanes from a when the mask is true, otherwise b, preserving exact words.
    template<class M> requires (std::same_as<M,mask_type> || std::same_as<M,vector_mask_type>)
    simd_nodiscard friend simd_inline simd_const simd select(M mask,simd a,simd b) noexcept {
      // Both mask domains select whole lanes, preserving chosen NaN payloads.
      return {canonical{},select(mask,a.value_,b.value_)};
    }
    /// \brief Scales active lanes by 2^floor(exponent), preserving prior lanes elsewhere
    /// under the FTZ contract.
    template <class M, class A, class B, class E>
      requires (std::same_as<M,mask_type> || std::same_as<M,vector_mask_type>) &&
        (std::same_as<A,simd> || std::same_as<B,simd>) &&
        std::convertible_to<A,simd> && std::convertible_to<B,simd> &&
        (std::same_as<E,simd> || std::convertible_to<E,V>)
    simd_nodiscard friend simd_inline simd masked_scaleb(
        M active, A prior, B value, E exponent)
        noexcept(noexcept(simd(prior)) && noexcept(simd(value)) &&
          noexcept(scaling_exponent(exponent))) {
      simd imported_prior(prior), imported_value(value);
      V shift = scaling_exponent(exponent);
      V result = masked_scaleb(active, imported_prior.value_, imported_value.value_, shift);
      return {canonical{},::ftz::detail::ftz32_scaled_result<Hardware>(active,result,imported_value.value_,shift)};
    }
    /// \brief Scales active lanes by 2^floor(exponent) and returns positive zero in inactive lanes.
    template <class M, class A, class E>
      requires (std::same_as<M,mask_type> || std::same_as<M,vector_mask_type>) &&
        std::same_as<A,simd> && (std::same_as<E,simd> || std::convertible_to<E,V>)
    simd_nodiscard friend simd_inline simd masked_scaleb_zero(
        M active, A value, E exponent) noexcept(noexcept(scaling_exponent(exponent))) {
      V shift = scaling_exponent(exponent);
      V result = masked_scaleb_zero(active,value.value_,shift);
      return {canonical{},::ftz::detail::ftz32_scaled_result<Hardware>(active,result,value.value_,shift)};
    }
    /// \brief Scales each lane by 2^floor(exponent), retaining the FTZ result and boundary repair.
    template <class A, class E> requires std::same_as<A,simd> &&
      (std::same_as<E,simd> || std::convertible_to<E,V>)
    simd_nodiscard friend simd_inline simd scaleb(A value, E exponent)
        noexcept(noexcept(scaling_exponent(exponent))) {
      V shift = scaling_exponent(exponent);
      V result = scaleb(value.value_,shift);
      return {canonical{},::ftz::detail::ftz32_scaled_result<Hardware>(mask_type(true),result,value.value_,shift)};
    }
    // An FTZ exponent alone does not change a raw base's arithmetic contract.
    // Explicit forwarding also prevents competing implicit native conversions.
    /// \brief Forwards an FTZ exponent to raw scaling; the raw base and result retain their
    /// raw arithmetic contract.
    template <class M, class A, class B>
      requires (std::same_as<M,mask_type> || std::same_as<M,vector_mask_type>) &&
        std::same_as<A,V> && std::same_as<B,V>
    simd_nodiscard friend simd_inline simd_const V masked_scaleb(
        M active, A prior, B value, simd exponent) noexcept {
      return masked_scaleb(active,prior,value,exponent.value_);
    }
    /// \brief Forwards an FTZ exponent to raw masked scaling with inactive lanes zeroed.
    template <class M, class A>
      requires (std::same_as<M,mask_type> || std::same_as<M,vector_mask_type>) && std::same_as<A,V>
    simd_nodiscard friend simd_inline simd_const V masked_scaleb_zero(
        M active, A value, simd exponent) noexcept {
      return masked_scaleb_zero(active,value,exponent.value_);
    }
    /// \brief Forwards an FTZ exponent to raw scaling without changing the raw base or result contract.
    template <class A> requires std::same_as<A,V>
    simd_nodiscard friend simd_inline simd_const V scaleb(A value, simd exponent) noexcept {
      return scaleb(value,exponent.value_);
    }
    /// \brief Evaluates the scalar FTZ square-root graph per lane, retaining signed zero and
    /// special values.
    simd_nodiscard friend simd_inline simd_const simd sqrt(simd a) noexcept {
      return {canonical{}, ::ftz::detail::ftz32_vector_sqrt(a.value_)};
    }
    /// \brief Evaluates fused a*b+c in this FTZ policy; converting operands can throw as
    /// specified by noexcept.
    template <class A, class B> requires std::convertible_to<A, simd> && std::convertible_to<B, simd>
    simd_nodiscard friend simd_inline simd fma(simd a, A b, B c)
        noexcept(noexcept(simd(b)) && noexcept(simd(c))) {
      return fused(a, simd(b), simd(c));
    }
    /// \brief Evaluates fused a*b+c in this FTZ policy; converting operands can throw as
    /// specified by noexcept.
    template <class A, class B> requires (!std::same_as<A, simd>) && std::convertible_to<A, simd> && std::convertible_to<B, simd>
    simd_nodiscard friend simd_inline simd fma(A a, simd b, B c)
        noexcept(noexcept(simd(a)) && noexcept(simd(c))) {
      return fused(simd(a), b, simd(c));
    }
    /// \brief Evaluates fused a*b+c in this FTZ policy; converting operands can throw as
    /// specified by noexcept.
    template <class A, class B> requires (!std::same_as<A, simd>) && (!std::same_as<B, simd>) &&
        std::convertible_to<A, simd> && std::convertible_to<B, simd>
    simd_nodiscard friend simd_inline simd fma(A a, B b, simd c)
        noexcept(noexcept(simd(a)) && noexcept(simd(b))) {
      return fused(simd(a), simd(b), c);
    }

    // Mixed operands select this value contract instead of escaping via the
    // deliberately implicit native-register conversion. No runtime dispatch.
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline simd operator+(simd a, T b)
        noexcept(noexcept(a + simd(b))) { return a + simd(b); }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline simd operator+(T a, simd b)
        noexcept(noexcept(simd(a) + b)) { return simd(a) + b; }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline simd operator-(simd a, T b)
        noexcept(noexcept(a - simd(b))) { return a - simd(b); }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline simd operator-(T a, simd b)
        noexcept(noexcept(simd(a) - b)) { return simd(a) - b; }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline simd operator*(simd a, T b)
        noexcept(noexcept(a * simd(b))) { return a * simd(b); }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline simd operator*(T a, simd b)
        noexcept(noexcept(simd(a) * b)) { return simd(a) * b; }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline simd operator/(simd a, T b)
        noexcept(noexcept(a / simd(b))) { return a / simd(b); }
    /// \brief Evaluates lane arithmetic in this FTZ policy; converting operands retain their
    /// conditional noexcept.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline simd operator/(T a, simd b)
        noexcept(noexcept(simd(a) / b)) { return simd(a) / b; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator<(simd a, T b)
        noexcept(noexcept(a < simd(b))) { return a < simd(b); }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator<(T a, simd b)
        noexcept(noexcept(simd(a) < b)) { return simd(a) < b; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator>(simd a, T b)
        noexcept(noexcept(a > simd(b))) { return a > simd(b); }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator>(T a, simd b)
        noexcept(noexcept(simd(a) > b)) { return simd(a) > b; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator==(simd a, T b)
        noexcept(noexcept(a == simd(b))) { return a == simd(b); }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator==(T a, simd b)
        noexcept(noexcept(simd(a) == b)) { return simd(a) == b; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator!=(simd a, T b)
        noexcept(noexcept(a != simd(b))) { return a != simd(b); }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator!=(T a, simd b)
        noexcept(noexcept(simd(a) != b)) { return simd(a) != b; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator<=(simd a, T b)
        noexcept(noexcept(a <= simd(b))) { return a <= simd(b); }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator<=(T a, simd b)
        noexcept(noexcept(simd(a) <= b)) { return simd(a) <= b; }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator>=(simd a, T b)
        noexcept(noexcept(a >= simd(b))) { return a >= simd(b); }
    /// \brief Compares lanes after conversion to this policy, returning the
    /// architecture-native mask type.
    template <class T> requires (!std::same_as<T, simd>) && std::convertible_to<T, simd>
    simd_nodiscard friend simd_inline mask_type operator>=(T a, simd b)
        noexcept(noexcept(simd(a) >= b)) { return simd(a) >= b; }

  private:
    
    using bridge = ::ftz::detail::ftz32_bridge<V>;
    template <class E>
    simd_nodiscard static simd_inline V scaling_exponent(E exponent)
        noexcept(noexcept(V(exponent))) {
      if constexpr (std::same_as<E,simd>) return exponent.value_;
      else {
        V raw(exponent);
        if constexpr(Hardware) return raw;
        else return bridge::decode(::ftz::detail::ftz32_import_words(bridge::encode(raw)));
      }
    }
    simd_nodiscard static simd_inline simd_const V import(V value) noexcept {
      return bridge::decode(::ftz::detail::ftz32_import_words(bridge::encode(value)));

    }
    struct canonical {};
    V value_;
    simd_inline simd_customization(canonical, V value) noexcept : value_(value) {}
    simd_nodiscard static simd_inline simd_const simd fused(simd a, simd b, simd c) noexcept {
      return {canonical{}, ::ftz::detail::ftz32_vector_fma(a.value_, b.value_, c.value_)};
    }
  };
}

export namespace ftz {
  // A scalar FTZ operand promotes a raw register even when no FTZ SIMD
  // argument exists for hidden-friend lookup. Never fall back to built-in float.
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns its FTZ
  /// vector rebind.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator+(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) + detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns its FTZ
  /// vector rebind.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator+(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) + detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns its FTZ
  /// vector rebind.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator-(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) - detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns its FTZ
  /// vector rebind.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator-(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) - detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns its FTZ
  /// vector rebind.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator*(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) * detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns its FTZ
  /// vector rebind.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator*(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) * detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns its FTZ
  /// vector rebind.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator/(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) / detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns its FTZ
  /// vector rebind.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator/(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) / detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator<(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) < detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator<(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) < detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator>(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) > detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator>(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) > detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator==(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) == detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator==(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) == detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator!=(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) != detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator!=(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) != detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator<=(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) <= detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator<=(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) <= detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator>=(V a,basic_ftz32<Hardware> b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) >= detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports the raw register into the scalar operand's FTZ policy and returns the
  /// native comparison mask.
  template <bool Hardware, detail::raw_register V>
  simd_nodiscard simd_inline simd_const auto operator>=(basic_ftz32<Hardware> a,V b) noexcept {
    return detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) >= detail::ftz32_simd<V,basic_ftz32<Hardware>>(b);
  }
  // Compound assignment keeps the destination type, while the FTZ operand
  // still chooses the arithmetic contract before publication to raw storage.
  /// \ingroup ftz_vectors
  /// \brief Evaluates FTZ arithmetic before assigning to the existing raw-register destination.
  template <bool Hardware, detail::raw_register V>
  simd_inline V & operator+=(V & a,basic_ftz32<Hardware> b) noexcept {
    a=(detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) + detail::ftz32_simd<V,basic_ftz32<Hardware>>(b)).to_native();
    return a;
  }
  /// \ingroup ftz_vectors
  /// \brief Evaluates FTZ arithmetic before assigning to the existing raw-register destination.
  template <detail::raw_register V, detail::ftz32_vector R> requires std::same_as<typename R::register_type,V>
  simd_inline V & operator+=(V & a,R b) noexcept {
    a=(R(a) + R(b)).to_native();
    return a;
  }
  /// \ingroup ftz_vectors
  /// \brief Evaluates FTZ arithmetic before assigning to the existing raw-register destination.
  template <bool Hardware, detail::raw_register V>
  simd_inline V & operator-=(V & a,basic_ftz32<Hardware> b) noexcept {
    a=(detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) - detail::ftz32_simd<V,basic_ftz32<Hardware>>(b)).to_native();
    return a;
  }
  /// \ingroup ftz_vectors
  /// \brief Evaluates FTZ arithmetic before assigning to the existing raw-register destination.
  template <detail::raw_register V, detail::ftz32_vector R> requires std::same_as<typename R::register_type,V>
  simd_inline V & operator-=(V & a,R b) noexcept {
    a=(R(a) - R(b)).to_native();
    return a;
  }
  /// \ingroup ftz_vectors
  /// \brief Evaluates FTZ arithmetic before assigning to the existing raw-register destination.
  template <bool Hardware, detail::raw_register V>
  simd_inline V & operator*=(V & a,basic_ftz32<Hardware> b) noexcept {
    a=(detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) * detail::ftz32_simd<V,basic_ftz32<Hardware>>(b)).to_native();
    return a;
  }
  /// \ingroup ftz_vectors
  /// \brief Evaluates FTZ arithmetic before assigning to the existing raw-register destination.
  template <detail::raw_register V, detail::ftz32_vector R> requires std::same_as<typename R::register_type,V>
  simd_inline V & operator*=(V & a,R b) noexcept {
    a=(R(a) * R(b)).to_native();
    return a;
  }
  /// \ingroup ftz_vectors
  /// \brief Evaluates FTZ arithmetic before assigning to the existing raw-register destination.
  template <bool Hardware, detail::raw_register V>
  simd_inline V & operator/=(V & a,basic_ftz32<Hardware> b) noexcept {
    a=(detail::ftz32_simd<V,basic_ftz32<Hardware>>(a) / detail::ftz32_simd<V,basic_ftz32<Hardware>>(b)).to_native();
    return a;
  }
  /// \ingroup ftz_vectors
  /// \brief Evaluates FTZ arithmetic before assigning to the existing raw-register destination.
  template <detail::raw_register V, detail::ftz32_vector R> requires std::same_as<typename R::register_type,V>
  simd_inline V & operator/=(V & a,R b) noexcept {
    a=(R(a) / R(b)).to_native();
    return a;
  }
  namespace detail {
    template <class... T> struct raw_family { using type = void; };
    template <class T,class... Rest> struct raw_family<T,Rest...> {
      using type = std::conditional_t<raw_register<T>,T,typename raw_family<Rest...>::type>;
    };
    template <class... X> using raw_family_t = typename raw_family<X...>::type;
    template <class... T> struct scalar_family { using type = void; };
    template <class T,class... Rest> struct scalar_family<T,Rest...> {
      using type = std::conditional_t<ftz32_type<T>,T,typename scalar_family<Rest...>::type>;
    };
    template <class... X> using scalar_family_t = typename scalar_family<X...>::type;
    template <class... X> concept ftz32_raw_scalar_fma =
      (ftz32_type<std::remove_cvref_t<X>> || ...) &&
      raw_register<raw_family_t<X...>> &&
      (std::convertible_to<X &,ftz32_simd<raw_family_t<X...>,scalar_family_t<X...>>> && ...);
  }
  /// \ingroup ftz_vectors
  /// \brief Imports raw SIMD operands into the FTZ scalar operand's policy and evaluates fused a*b+c.
  /// Returns the raw register's matching FTZ vector rebind; operand conversions determine noexcept.
  template <class A,class B,class C> requires detail::ftz32_raw_scalar_fma<A,B,C>
  simd_nodiscard simd_inline auto fma(A a,B b,C c)
      noexcept(std::is_nothrow_constructible_v<detail::ftz32_simd<detail::raw_family_t<A,B,C>,detail::scalar_family_t<A,B,C>>,A &> &&
        std::is_nothrow_constructible_v<detail::ftz32_simd<detail::raw_family_t<A,B,C>,detail::scalar_family_t<A,B,C>>,B &> &&
        std::is_nothrow_constructible_v<detail::ftz32_simd<detail::raw_family_t<A,B,C>,detail::scalar_family_t<A,B,C>>,C &>) {
    using result_type=detail::ftz32_simd<detail::raw_family_t<A,B,C>,detail::scalar_family_t<A,B,C>>;
    return fma(result_type(a),result_type(b),result_type(c));
  }
}

#if !defined(__cpp_structured_bindings) || __cpp_structured_bindings < 202411L
#error "FTZ array math requires C++26 structured-binding packs (Clang 21+ with -std=c++2c or clang-cl /std:c++latest)."
#endif

export namespace ftz {
  namespace detail {
    template <class R> struct ftz32_native_for;
    template <ftz32_vector R> struct ftz32_native_for<R> { using type = typename R::register_type; };
    template <bool H> struct ftz32_native_for<basic_ftz32<H>> { using type = ::ftz::detail::native::fp32x1; };
    template <class R> using ftz32_native = typename ftz32_native_for<R>::type;
    template <class R> concept ftz32_value = requires { typename ftz32_native_for<R>::type; };
    template <ftz32_value R> simd_nodiscard simd_inline simd_const ftz32_native<R> ftz32_unwrap(R value) noexcept {
      if constexpr (ftz32_type<R>) return ::ftz::detail::native::fp32x1(value.to_float());
      else return value.to_native();
    }
    template <ftz32_value R> simd_nodiscard simd_inline simd_const R ftz32_wrap(ftz32_native<R> value) noexcept {
      if constexpr (ftz32_type<R>) return R::unsafe_from_float32(value.value);
      else return R::unsafe_from_float32(value);
    }

  }

  namespace detail::ftz32_math {
    // All register chains enter the existing stage-interleaved polynomial in one
    // call. Only out-of-domain lanes use the scalar full-range/special-value path.
    template <detail::ftz32_value R, std::size_t N>
    simd_nodiscard simd_inline simd_pure auto sincos(std::array<R, N> const & input) noexcept {
      if constexpr (N == 0) return std::pair{std::array<R,0>{}, std::array<R,0>{}};
      else {
        using V = detail::ftz32_native<R>; using B = detail::ftz32_bridge<V>; using U = detail::ftz32_words<V>;
        auto const & [...input_register] = input;
        auto const [...original] = std::array{detail::ftz32_unwrap(input_register)...};
        auto bounded = [](V value) {
          return mask_bits<std::uint32_t>(U(0x46000000u) > (B::encode(value) & U(0x7fffffffu)));
        };
        auto const [...allowed] = std::array{bounded(original)...};
        auto [sine_values, cosine_values] = ::ftz::detail::native::sincos_ftz<R::hardware>(
          std::array{B::decode(B::encode(original) & allowed)...});
        auto const & [...sine] = sine_values;
        auto const & [...cosine] = cosine_values;
        return std::pair{
          std::array{detail::ftz32_wrap<R>(detail::ftz32_repair<detail::ftz32_sin<R::hardware>>(
            sine, allowed ^ U(0xffffffffu), original))...},
          std::array{detail::ftz32_wrap<R>(detail::ftz32_repair<detail::ftz32_cos<R::hardware>>(
            cosine, allowed ^ U(0xffffffffu), original))...}};
      }
    }
    template <bool Cosine, detail::ftz32_value R, std::size_t N>
    simd_nodiscard simd_inline simd_pure std::array<R, N> trig_single(std::array<R, N> const & input) noexcept {
      if constexpr (N == 0) return {};
      else {
        using V = detail::ftz32_native<R>; using B = detail::ftz32_bridge<V>; using U = detail::ftz32_words<V>;
        auto const & [...input_register] = input;
        auto const [...original] = std::array{detail::ftz32_unwrap(input_register)...};
        auto const [...allowed] = std::array{mask_bits<std::uint32_t>(
            U(0x46000000u) > (B::encode(original) & U(0x7fffffffu)))...};
        auto const safe = std::array{B::decode(B::encode(original) & allowed)...};
        auto const [...value] = [&] {
          if constexpr (Cosine) return native::cos_ftz<R::hardware>(safe);
          else return native::sin_ftz<R::hardware>(safe);
        }();
        if constexpr (Cosine)
          return std::array{detail::ftz32_wrap<R>(detail::ftz32_repair<detail::ftz32_cos<R::hardware>>(
            value, allowed ^ U(0xffffffffu), original))...};
        else
          return std::array{detail::ftz32_wrap<R>(detail::ftz32_repair<detail::ftz32_sin<R::hardware>>(
            value, allowed ^ U(0xffffffffu), original))...};
      }
    }
    template <detail::ftz32_value R, std::size_t N>
    simd_nodiscard simd_inline simd_pure std::array<R, N> sin(std::array<R, N> const & input) noexcept { return trig_single<false>(input); }
    template <detail::ftz32_value R, std::size_t N>
    simd_nodiscard simd_inline simd_pure std::array<R, N> cos(std::array<R, N> const & input) noexcept { return trig_single<true>(input); }
    template <detail::ftz32_value R, std::size_t N>
    simd_nodiscard simd_inline simd_pure std::array<R, N> exp(std::array<R, N> const & input) noexcept {
      if constexpr (N == 0) return {};
      else {
        auto const & [...input_register] = input;
        using ::simd::exp;
        auto const [...value] = exp(
          std::array{detail::ftz32_unwrap(input_register)...}, std::true_type{});
        return std::array{detail::ftz32_wrap<R>(value)...};
      }
    }
    template <detail::ftz32_value R, std::size_t N>
    simd_nodiscard simd_inline simd_pure std::array<R, N> expm1(std::array<R, N> const & input) noexcept {
      if constexpr (N == 0) return {};
      else {
        using U = detail::ftz32_words<detail::ftz32_native<R>>;
        auto const & [...input_register] = input;
        auto original = std::array{detail::ftz32_unwrap(input_register)...};
        auto [values, validity] = ::ftz::detail::native::expm1_checked<R::hardware>(original);
        auto const & [...x] = original;
        auto const & [...value] = values;
        auto const & [...valid] = validity;
        // The native graph classifies and safely evaluates once. Its validity is
        // 0/1 per lane; subtraction maps invalid lanes to the full repair mask.
        return std::array{detail::ftz32_wrap<R>(detail::ftz32_repair<detail::ftz32_expm1<R::hardware>>(
          value, valid - U(1), x))...};
      }
    }
  }
  /// \ingroup ftz_vectors
  /// \brief Computes sine and cosine in radians with the scalar special-value rules; returns a
  /// pair in that order. Both members have type R.
  template <detail::ftz32_vector R>
  simd_nodiscard simd_inline simd_pure auto sincos(R input) noexcept {
    auto [sine_values, cosine_values] = detail::ftz32_math::sincos(std::array{input});
    auto [sine] = sine_values;
    auto [cosine] = cosine_values;
    return std::pair{sine, cosine};
  }
  /// \ingroup ftz_vectors
  /// \brief Computes sine in radians with dedicated output reconstruction and the scalar FTZ
  /// special-value rules. Returns R.
  template <detail::ftz32_vector R>
  simd_nodiscard simd_inline simd_pure R sin(R input) noexcept { return detail::ftz32_math::sin(std::array{input})[0]; }
  /// \ingroup ftz_vectors
  /// \brief Computes cosine in radians with dedicated output reconstruction and the scalar FTZ
  /// special-value rules. Returns R.
  template <detail::ftz32_vector R>
  simd_nodiscard simd_inline simd_pure R cos(R input) noexcept { return detail::ftz32_math::cos(std::array{input})[0]; }
  /// \ingroup ftz_vectors
  /// \brief Computes the exponential with the scalar FTZ underflow, overflow and special-value
  /// rules. Returns R.
  template <detail::ftz32_vector R>
  simd_nodiscard simd_inline simd_pure R exp(R input) noexcept {
    auto [value] = detail::ftz32_math::exp(std::array{input});
    return value;
  }
  /// \ingroup ftz_vectors
  /// \brief Computes exp(x)-1 with the scalar FTZ graph, preserving signed zero. Returns R.
  template <detail::ftz32_vector R>
  simd_nodiscard simd_inline simd_pure R expm1(R input) noexcept {
    auto [value] = detail::ftz32_math::expm1(std::array{input});
    return value;
  }
}


export namespace ftz {
  namespace detail {
    template <class R> inline constexpr bool ftz32_vector_value = ftz32_vector<R>;
    template <char Op, class V> simd_nodiscard simd_inline simd_const V ftz32_native_binary(V a,V b) noexcept {
      if constexpr(Op=='+') return a+b;
      else if constexpr(Op=='-') return a-b;
      else return a*b;
    }
    template <char Op, class R, std::size_t N>
    simd_nodiscard simd_inline simd_pure std::array<R,N> ftz32_array_binary(
        std::array<R,N> const & a, std::array<R,N> const & b) noexcept {
      if constexpr (N == 0) return {};
      else {
        auto const & [...x] = a;
        auto const & [...y] = b;
        // Issue every native operation before classifying or repairing lanes.
        auto const [...native] = std::array{ftz32_native_binary<Op>(x.to_native(), y.to_native())...};
        if constexpr (R::hardware && (Op == '+' || Op == '-'))
          return std::array{R::unsafe_from_float32(native)...};
        auto const [...mask] = std::array{ftz32_repair_mask(native, x.to_native(), y.to_native())...};
        constexpr auto repair = [] {
          if constexpr (Op == '+') return ftz32_add<R::hardware>;
          else if constexpr (Op == '-') return ftz32_sub<R::hardware>;
          else return ftz32_mul;
        }();
        return std::array{R::unsafe_from_float32(ftz32_repair<repair>(
          native, mask, x.to_native(), y.to_native()))...};
      }
    }
    template <class R, std::size_t N>
    simd_nodiscard simd_inline simd_pure std::array<R,N> ftz32_array_fma(std::array<R,N> const & a,
        std::array<R,N> const & b, std::array<R,N> const & c) noexcept {
      if constexpr (N == 0) return {};
      else {
        auto const & [...x] = a;
        auto const & [...y] = b;
        auto const & [...z] = c;
        auto const [...native] = std::array{fma(x.to_native(), y.to_native(), z.to_native())...};
        auto const [...mask] = std::array{ftz32_repair_mask(
          native, x.to_native(), y.to_native(), z.to_native())...};
        return std::array{R::unsafe_from_float32(ftz32_repair<ftz32_fma>(
          native, mask, x.to_native(), y.to_native(), z.to_native()))...};
      }
    }
  }
  namespace detail {
    template<class T, std::size_t N> simd_inline std::array<T,N> array_broadcast(T const & value) noexcept {
      std::array<T,N> result;
      auto & [...element] = result;
      ((element = value), ...);
      return result;
    }
  }
  /// \ingroup ftz_register_arrays
  /// \brief Adds matching register arrays; a non-array operand is converted once and broadcast,
  /// with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N> requires detail::ftz32_vector_value<R>
  simd_nodiscard simd_inline std::array<R,N> add(std::array<R,N> const & a, std::array<R,N> const & b) noexcept {
    return detail::ftz32_array_binary<'+'>(a,b);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Adds matching register arrays; a non-array operand is converted once and broadcast,
  /// with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N, class B> requires detail::ftz32_vector_value<R> && std::convertible_to<B const &,R>
  simd_nodiscard simd_inline std::array<R,N> add(std::array<R,N> const & a, B const & b)
      noexcept(std::is_nothrow_constructible_v<R,B const &>) {
    return add(a, detail::array_broadcast<R,N>(R(b)));
  }
  /// \ingroup ftz_register_arrays
  /// \brief Adds matching register arrays; a non-array operand is converted once and broadcast,
  /// with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N, class A> requires detail::ftz32_vector_value<R> && std::convertible_to<A const &,R>
  simd_nodiscard simd_inline std::array<R,N> add(A const & a, std::array<R,N> const & b)
      noexcept(std::is_nothrow_constructible_v<R,A const &>) {
    return add(detail::array_broadcast<R,N>(R(a)), b);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Subtracts matching register arrays; a non-array operand is converted once and
  /// broadcast, with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N> requires detail::ftz32_vector_value<R>
  simd_nodiscard simd_inline std::array<R,N> sub(std::array<R,N> const & a, std::array<R,N> const & b) noexcept {
    return detail::ftz32_array_binary<'-'>(a,b);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Subtracts matching register arrays; a non-array operand is converted once and
  /// broadcast, with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N, class B> requires detail::ftz32_vector_value<R> && std::convertible_to<B const &,R>
  simd_nodiscard simd_inline std::array<R,N> sub(std::array<R,N> const & a, B const & b)
      noexcept(std::is_nothrow_constructible_v<R,B const &>) {
    return sub(a, detail::array_broadcast<R,N>(R(b)));
  }
  /// \ingroup ftz_register_arrays
  /// \brief Subtracts matching register arrays; a non-array operand is converted once and
  /// broadcast, with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N, class A> requires detail::ftz32_vector_value<R> && std::convertible_to<A const &,R>
  simd_nodiscard simd_inline std::array<R,N> sub(A const & a, std::array<R,N> const & b)
      noexcept(std::is_nothrow_constructible_v<R,A const &>) {
    return sub(detail::array_broadcast<R,N>(R(a)), b);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Multiplies matching register arrays; a non-array operand is converted once and
  /// broadcast, with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N> requires detail::ftz32_vector_value<R>
  simd_nodiscard simd_inline std::array<R,N> mul(std::array<R,N> const & a, std::array<R,N> const & b) noexcept {
    return detail::ftz32_array_binary<'*'>(a,b);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Multiplies matching register arrays; a non-array operand is converted once and
  /// broadcast, with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N, class B> requires detail::ftz32_vector_value<R> && std::convertible_to<B const &,R>
  simd_nodiscard simd_inline std::array<R,N> mul(std::array<R,N> const & a, B const & b)
      noexcept(std::is_nothrow_constructible_v<R,B const &>) {
    return mul(a, detail::array_broadcast<R,N>(R(b)));
  }
  /// \ingroup ftz_register_arrays
  /// \brief Multiplies matching register arrays; a non-array operand is converted once and
  /// broadcast, with conversion noexcept retained. Returns std::array<R,N>.
  template<class R, std::size_t N, class A> requires detail::ftz32_vector_value<R> && std::convertible_to<A const &,R>
  simd_nodiscard simd_inline std::array<R,N> mul(A const & a, std::array<R,N> const & b)
      noexcept(std::is_nothrow_constructible_v<R,A const &>) {
    return mul(detail::array_broadcast<R,N>(R(a)), b);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Evaluates fused a*b+c in one FTZ policy; array results retain the same element type
  /// and extent. Returns std::array<R,N>.
  template<class R, std::size_t N> requires detail::ftz32_vector_value<R>
  simd_nodiscard simd_inline std::array<R,N> fma(std::array<R,N> const & a,
      std::array<R,N> const & b, std::array<R,N> const & c) noexcept {
    return detail::ftz32_array_fma(a,b,c);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Computes sine in radians with dedicated output reconstruction and the scalar FTZ
  /// special-value rules. Returns std::array<R,N>.
  template<detail::ftz32_value R, std::size_t N>
  simd_nodiscard simd_inline auto sin(std::array<R,N> const & input) noexcept {
    return detail::ftz32_math::sin(input);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Computes cosine in radians with dedicated output reconstruction and the scalar FTZ
  /// special-value rules. Returns std::array<R,N>.
  template<detail::ftz32_value R, std::size_t N>
  simd_nodiscard simd_inline auto cos(std::array<R,N> const & input) noexcept {
    return detail::ftz32_math::cos(input);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Computes the exponential with the scalar FTZ underflow, overflow and special-value
  /// rules. Returns std::array<R,N>.
  template<detail::ftz32_value R, std::size_t N>
  simd_nodiscard simd_inline auto exp(std::array<R,N> const & input) noexcept {
    return detail::ftz32_math::exp(input);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Computes exp(x)-1 with the scalar FTZ graph, preserving signed zero. Returns std::array<R,N>.
  template<detail::ftz32_value R, std::size_t N>
  simd_nodiscard simd_inline auto expm1(std::array<R,N> const & input) noexcept {
    return detail::ftz32_math::expm1(input);
  }
  /// \ingroup ftz_register_arrays
  /// \brief Computes sine and cosine in radians with the scalar special-value rules; returns a
  /// pair in that order. Each member is std::array<R,N>.
  template<detail::ftz32_value R, std::size_t N>
  simd_nodiscard simd_inline auto sincos(std::array<R,N> const & input) noexcept {
    return detail::ftz32_math::sincos(input);
  }

}

// SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0

export namespace ftz {
  /// \ingroup ftz_register_arrays
  /// \brief Computes hyperbolic tangent with the scalar FTZ graph, preserving signed zero and
  /// saturating infinities. Returns std::array<R,N>.
  template<detail::ftz32_value R,std::size_t N>
  simd_nodiscard simd_inline std::array<R,N> tanh(std::array<R,N> const & input) noexcept {
    if constexpr(N==0) return {};
    else {
      auto const & [...value]=input;
      auto const [...result]=detail::native::tanh_ftz<R::hardware>(
        std::array{detail::ftz32_unwrap(value)...});
      return {{detail::ftz32_wrap<R>(result)...}};
    }
  }
  /// \ingroup ftz_vectors
  /// \brief Computes hyperbolic tangent with the scalar FTZ graph, preserving signed zero and
  /// saturating infinities. Returns R.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline R tanh(R input) noexcept {
    auto [result]=::ftz::tanh(std::array{input});
    return result;
  }
}

export namespace ftz {
  /// \ingroup ftz_register_arrays
  /// \brief Computes natural logarithms; either zero gives negative infinity, negative nonzero
  /// values give NaN. Returns std::array<R,N>.
  template<detail::ftz32_value R,std::size_t N>
  simd_nodiscard simd_inline std::array<R,N> log(std::array<R,N> const & input) noexcept {
    if constexpr(N==0) return {};
    else {
      auto const & [...value]=input;
      auto const [...result]=detail::native::log_ftz<false,R::hardware>(
        std::array{detail::ftz32_unwrap(value)...});
      return {{detail::ftz32_wrap<R>(result)...}};
    }
  }
  /// \ingroup ftz_vectors
  /// \brief Computes natural logarithms; either zero gives negative infinity, negative nonzero
  /// values give NaN. Returns R.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline R log(R input) noexcept {
    auto [result]=::ftz::log(std::array{input});
    return result;
  }
  /// \ingroup ftz_register_arrays
  /// \brief Computes log(1+x), preserving signed zero; -1 gives negative infinity and x < -1
  /// gives NaN. Returns std::array<R,N>.
  template<detail::ftz32_value R,std::size_t N>
  simd_nodiscard simd_inline std::array<R,N> log1p(std::array<R,N> const & input) noexcept {
    if constexpr(N==0) return {};
    else {
      auto const & [...value]=input;
      auto const [...result]=detail::native::log_ftz<true,R::hardware>(
        std::array{detail::ftz32_unwrap(value)...});
      return {{detail::ftz32_wrap<R>(result)...}};
    }
  }
  /// \ingroup ftz_vectors
  /// \brief Computes log(1+x), preserving signed zero; -1 gives negative infinity and x < -1
  /// gives NaN. Returns R.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline R log1p(R input) noexcept {
    auto [result]=::ftz::log1p(std::array{input});
    return result;
  }
}

export namespace ftz {
  /// \ingroup ftz_register_arrays
  /// \brief Computes atan2(y,x) in radians with the scalar FTZ signed-axis and infinity rules.
  /// Returns std::array<R,N>.
  template<detail::ftz32_value R,std::size_t N>
  simd_nodiscard simd_inline std::array<R,N> atan2(
      std::array<R,N> const & y,std::array<R,N> const & x) noexcept {
    if constexpr(N==0) return {};
    else {
      auto const & [...a]=y;
      auto const & [...b]=x;
      auto const [...result]=detail::native::atan2_ftz<R::hardware>(
        std::array{detail::ftz32_unwrap(a)...},std::array{detail::ftz32_unwrap(b)...});
      return {{detail::ftz32_wrap<R>(result)...}};
    }
  }
  /// \ingroup ftz_vectors
  /// \brief Computes atan2(y,x) in radians with the scalar FTZ signed-axis and infinity rules. Returns R.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline R atan2(R y,R x) noexcept {
    auto [result]=::ftz::atan2(std::array{y},std::array{x});
    return result;
  }
}

export namespace ftz {
  /// \ingroup ftz_vectors
  /// \brief Returns R::mask for NaN lanes by inspecting words; no FP evaluation or NaN quieting occurs.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline typename R::mask isnan(R value) noexcept {
    using U=typename R::bits_type;
    return (value.to_bits() & U(0x7fffffffu)) > U(0x7f800000u);
  }
  /// \ingroup ftz_vectors
  /// \brief Returns R::mask for either infinity by inspecting lane words.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline typename R::mask isinf(R value) noexcept {
    using U=typename R::bits_type;
    return (value.to_bits() & U(0x7fffffffu)) == U(0x7f800000u);
  }
  /// \ingroup ftz_vectors
  /// \brief Returns R::mask for finite lanes by inspecting words, without changing FP status.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline typename R::mask isfinite(R value) noexcept {
    using U=typename R::bits_type;
    return (value.to_bits() & U(0x7fffffffu)) < U(0x7f800000u);
  }
  /// \ingroup ftz_vectors
  /// \brief Returns R::mask for set sign bits, including negative zero and signed NaNs.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline typename R::mask signbit(R value) noexcept {
    using U=typename R::bits_type;
    return (value.to_bits() & U(0x80000000u)) != U(0);
  }
  /// \ingroup ftz_vectors
  /// \brief Returns magnitude with the sign bits of sign; all other words, including NaN
  /// payloads, are preserved. Returns R.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline R copysign(R magnitude,R sign) noexcept {
    using U=typename R::bits_type;
    using V=typename R::register_type;
    return R::unsafe_from_float32(V::from_bits(
      (magnitude.to_bits() & U(0x7fffffffu)) | (sign.to_bits() & U(0x80000000u))));
  }
}

export namespace ftz {
  // Integral-valued results cannot be subnormal; preserve the raw rounding result.
  /// \ingroup ftz_vectors
  /// \brief Rounds toward negative infinity, independently of ambient rounding mode; signed
  /// zero and infinities survive. Returns R.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline R floor(R value) noexcept {
    using simd::floor;
    return R::unsafe_from_float32(floor(value.to_native()));
  }
  /// \ingroup ftz_register_arrays
  /// \brief Rounds toward negative infinity, independently of ambient rounding mode; signed
  /// zero and infinities survive. Returns std::array<R,N>.
  template<detail::ftz32_value R,std::size_t N>
  simd_nodiscard simd_inline std::array<R,N> floor(std::array<R,N> const & input) noexcept {
    auto const & [...value]=input;
    return {{floor(value)...}};
  }
  /// \ingroup ftz_vectors
  /// \brief Rounds toward positive infinity, independently of ambient rounding mode; signed
  /// zero and infinities survive. Returns R.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline R ceil(R value) noexcept {
    using simd::ceil;
    return R::unsafe_from_float32(ceil(value.to_native()));
  }
  /// \ingroup ftz_register_arrays
  /// \brief Rounds toward positive infinity, independently of ambient rounding mode; signed
  /// zero and infinities survive. Returns std::array<R,N>.
  template<detail::ftz32_value R,std::size_t N>
  simd_nodiscard simd_inline std::array<R,N> ceil(std::array<R,N> const & input) noexcept {
    auto const & [...value]=input;
    return {{ceil(value)...}};
  }
  /// \ingroup ftz_vectors
  /// \brief Rounds toward zero, independently of ambient rounding mode; signed zero and
  /// infinities survive. Returns R.
  template<detail::ftz32_vector R>
  simd_nodiscard simd_inline R trunc(R value) noexcept {
    using simd::trunc;
    return R::unsafe_from_float32(trunc(value.to_native()));
  }
  /// \ingroup ftz_register_arrays
  /// \brief Rounds toward zero, independently of ambient rounding mode; signed zero and
  /// infinities survive. Returns std::array<R,N>.
  template<detail::ftz32_value R,std::size_t N>
  simd_nodiscard simd_inline std::array<R,N> trunc(std::array<R,N> const & input) noexcept {
    auto const & [...value]=input;
    return {{trunc(value)...}};
  }
}
