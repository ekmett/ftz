#include "support/failure.h"
#include "math_contract.h"
#include <bit>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <ftz/ftz32_ops.h>
#include "support/imports.h"

namespace {
  using namespace native;
  using namespace ftz;
  namespace core = ftz::detail;
  void require(bool value, char const * message) {
    if (!value) ftz::test::fail(std::runtime_error(message));
  }
  template <class R> void expected(R value, std::array<std::uint32_t, R::lanes> const & words, char const * label) {
    std::array<std::uint32_t, R::lanes> actual;
    value.store_bits(actual.data());
    for (std::size_t lane = 0; lane < R::lanes; ++lane)
      if (!ftz::math_test::equivalent_fp32(actual[lane],words[lane]))
        ftz::test::fail(std::runtime_error(std::string(label) + " lane=" + std::to_string(lane) +
          " actual=" + std::to_string(actual[lane]) + " expected=" + std::to_string(words[lane])));
  }
  std::vector<std::array<std::uint32_t, 3>> bank() {
    std::uint32_t words[] = {0,0x80000000u,1,0x807fffffu,0x00800000u,0x80800000u,
      0x00800001u,0x3f7fffffu,0x20000001u,0x1ffffffeu,0x3f800000u,0x3f800001u,
      0x3f7ffffeu,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,
      0x7fc00000u,0x7f800001u,0xffa12345u,0x33000000u,0x39800000u,0x3f000000u};
    std::vector<std::array<std::uint32_t,3>> result;
    for (auto a : words) for (auto b : words) result.push_back({a,b,words[result.size()%std::size(words)]});
    result.push_back({0x3f800001u,0x3f7ffffeu,0xbf800000u});
    std::uint32_t state = 0xb2016a35u;
    auto next = [&] { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (unsigned i = 0; i < 2049; ++i) result.push_back({next(),next(),next()});
    return result;
  }
  // Rotate this bank through every lane/register. Each native register contains
  // ordinary values alongside full-range/special fallbacks, including both sides
  // of the sincos and expm1 fast-domain boundaries.
  template <class R, std::size_t N> void check_function_shape() {
    constexpr std::size_t lanes=[] {
      if constexpr (std::same_as<R,ftz32>) return std::size_t(1);
      else return R::lanes;
    }();
    constexpr std::uint32_t focused[] = {
      0x3e000000u,0x7f800000u,0xbf000000u,0x7fa12345u,
      0x00000000u,0x80000000u,0x00000001u,0x807fffffu,
      0x00800000u,0x80800000u,0x33000000u,0x33000001u,
      0xb3000000u,0xb3000001u,0x3f7fffffu,0x3f800000u,
      0x3f800001u,0xbf800001u,0x45ffffffu,0x46000000u,
      0x46000001u,0xc5ffffffu,0xc6000000u,0xc6000001u,
      0x42b17217u,0x42b17218u,0xc2aeac4fu,0xc2aeac50u,
      0xc2b40000u,0x461c4000u,0x7f7fffffu,0xff7fffffu,
      0xff800000u,0xffa12345u,0x7fc00000u,0x3f000000u
    };
    for (std::size_t offset=0;offset<(N==0?1:std::size(focused));++offset) {
      ::native::wide<R,N> input{};
      std::array<std::array<std::uint32_t,lanes>,N> words{};
      for (std::size_t reg=0;reg<N;++reg) {
        for (std::size_t lane=0;lane<lanes;++lane)
          words[reg][lane]=core::ftz32_canonical(focused[(offset+reg*7+lane)%std::size(focused)]);
        if constexpr (std::same_as<R,ftz32>) input.registers[reg]=ftz32::from_bits(words[reg][0]);
        else input.registers[reg]=R::load_bits(words[reg].data());
      }
      auto [sine_values,cosine_values]=sincos(input);
      auto exponential=exp(input);
      auto minus_one=expm1(input);
      static_assert(std::same_as<decltype(sine_values),::native::wide<R,N>>);
      static_assert(std::same_as<decltype(cosine_values),::native::wide<R,N>>);
      static_assert(std::same_as<decltype(exponential),::native::wide<R,N>>);
      static_assert(std::same_as<decltype(minus_one),::native::wide<R,N>>);
      require(sine_values.registers.size()==N && cosine_values.registers.size()==N &&
        exponential.registers.size()==N && minus_one.registers.size()==N,"wide function result shape");
      for (std::size_t reg=0;reg<N;++reg) {
        auto check=[&](R value,auto scalar,char const * label) {
          std::array<std::uint32_t,lanes> want{};
          for (std::size_t lane=0;lane<lanes;++lane) want[lane]=scalar(words[reg][lane]);
          if constexpr (std::same_as<R,ftz32>)
            require(ftz::math_test::equivalent_fp32(value.to_bits(),want[0]),label);
          else expected(value,want,label);
        };
        check(sine_values.registers[reg],core::ftz32_sin<>,"shaped wide sin");
        check(cosine_values.registers[reg],core::ftz32_cos<>,"shaped wide cos");
        check(exponential.registers[reg],core::ftz32_exp,"shaped wide exp");
        check(minus_one.registers[reg],core::ftz32_expm1<>,"shaped wide expm1");
        // Inputs are immutable raw state, including NaN payloads, even though
        // function-result NaNs compare by the declared equivalence relation.
        std::array<std::uint32_t,lanes> unchanged{};
        if constexpr (std::same_as<R,ftz32>) unchanged[0]=input.registers[reg].to_bits();
        else input.registers[reg].store_bits(unchanged.data());
        require(unchanged==words[reg],"wide function input changed");
      }
    }
  }
  template <class V> void check_functions() {
    using R = ::native::simd<ftz32,V::lanes,FTZ_TEST_ARCH>;
    check_function_shape<R,0>();
    check_function_shape<R,1>();
    check_function_shape<R,5>();
    if constexpr (V::lanes==8 || V::lanes==16) {
      static_assert((96/V::lanes)*V::lanes==96);
      check_function_shape<R,96/V::lanes>();
    }
    if constexpr (V::lanes==1) {
      check_function_shape<ftz32,0>();
      check_function_shape<ftz32,1>();
      check_function_shape<ftz32,5>();
    }
    auto rows = bank();
    for (std::size_t start = 0; start < rows.size(); start += R::lanes) {
      ::native::wide<R,3> input;
      std::array<std::array<std::uint32_t,R::lanes>,3> words{};
      for(std::size_t reg=0;reg<3;++reg) {
        for(std::size_t lane=0;lane<R::lanes && start+lane<rows.size();++lane)
          words[reg][lane]=core::ftz32_canonical(rows[start+lane][reg]);
        input.registers[reg]=R::load_bits(words[reg].data());
      }
      auto [sine_values,cosine_values]=sincos(input); auto exponential=exp(input); auto minus_one=expm1(input);
      for(std::size_t reg=0;reg<3;++reg) {
        std::array<std::uint32_t,R::lanes> want{};
        for(std::size_t lane=0;lane<R::lanes;++lane)want[lane]=core::ftz32_sin(words[reg][lane]);
        expected(sine_values.registers[reg],want,"wide sin");
        for(std::size_t lane=0;lane<R::lanes;++lane)want[lane]=core::ftz32_cos(words[reg][lane]);
        expected(cosine_values.registers[reg],want,"wide cos");
        for(std::size_t lane=0;lane<R::lanes;++lane)want[lane]=core::ftz32_exp(words[reg][lane]);
        expected(exponential.registers[reg],want,"wide exp");
        for(std::size_t lane=0;lane<R::lanes;++lane)want[lane]=core::ftz32_expm1(words[reg][lane]);
        expected(minus_one.registers[reg],want,"wide expm1");
      }
    }
    ::native::wide<ftz32,3> scalar{{ftz32(.125f),ftz32(-90.0f),ftz32(10000.0f)}};
    auto [sine_values,cosine_values]=sincos(scalar); auto e=exp(scalar); auto m=expm1(scalar);
    for(std::size_t i=0;i<3;++i) {
      auto word=scalar.registers[i].to_bits();
      require(sine_values.registers[i].to_bits()==core::ftz32_sin(word),"scalar-wide sin");
      require(cosine_values.registers[i].to_bits()==core::ftz32_cos(word),"scalar-wide cos");
      require(e.registers[i].to_bits()==core::ftz32_exp(word),"scalar-wide exp");
      require(m.registers[i].to_bits()==core::ftz32_expm1(word),"scalar-wide expm1");
    }
  }
  template <class... X> concept deduces_simd = requires (X... x) { ::native::simd{FTZ_TEST_ARCH,x...}; };
  static_assert(!deduces_simd<float> && !deduces_simd<ftz32>);
  template <class T> concept partial_import = requires (T const * p) { ::native::load_simd_partial<::native::simd<ftz32,1,FTZ_TEST_ARCH>>(p,0); };
  template <class T> concept partial_export = requires (T * p,::native::simd<ftz32,1,FTZ_TEST_ARCH> v) { ::native::store_simd_partial(p,v,0); };
  template <class T> struct recognize_simd;
  template <class T,std::size_t N,::native::isa Arch> struct recognize_simd<::native::simd<T,N,Arch>> {
    using value_type=T;static constexpr auto lanes=N;
  };
  void check_construction() {
    constexpr auto scalar_constant=::native::simd<float,1,FTZ_TEST_ARCH>{1.0f};
    static_assert(scalar_constant.value==1.0f);
    static_assert(std::same_as<decltype(::native::simd<float,1,FTZ_TEST_ARCH>{1.0f}),::native::simd<float,1,FTZ_TEST_ARCH>>);
    static_assert(std::same_as<decltype(::native::simd<ftz32,1,FTZ_TEST_ARCH>{ftz32(1.0f)}),::native::simd<ftz32,1,FTZ_TEST_ARCH>>);
    static_assert(std::same_as<decltype(::native::simd<float,2,FTZ_TEST_ARCH>{1.f,2.f}),::native::simd<float,2,FTZ_TEST_ARCH>>);
    static_assert(std::same_as<decltype(::native::simd<float,3,FTZ_TEST_ARCH>{1.f,2.f,3.f}),::native::simd<float,3,FTZ_TEST_ARCH>>);
    static_assert(!partial_import<int> && !partial_export<int>);
    static_assert(!std::constructible_from<::native::simd<ftz32,1,FTZ_TEST_ARCH>,std::array<int,1>>);
#if defined(__AVX2__) || defined(__ARM_NEON)
    auto raw=::native::simd<float,4,FTZ_TEST_ARCH>{1.f,2.f,3.f,4.f};
    auto canonical=::native::simd<ftz32,4,FTZ_TEST_ARCH>{1.f,ftz32(2),3.f,ftz32(4)};
    static_assert(std::same_as<decltype(raw),::native::simd<float,4,FTZ_TEST_ARCH>>);
    static_assert(std::same_as<decltype(canonical),::native::simd<ftz32,4,FTZ_TEST_ARCH>>);
    static_assert(recognize_simd<decltype(raw)>::lanes==4);
    static_assert(std::same_as<typename recognize_simd<decltype(canonical)>::value_type,ftz32>);
    static_assert(std::same_as<decltype(::native::simd{raw}),decltype(raw)>);
    static_assert(std::same_as<decltype(::native::simd{canonical}),decltype(canonical)>);
    static_assert(std::same_as<decltype(::native::simd<float,4,FTZ_TEST_ARCH>{raw.value}),decltype(raw)>);
    std::array<float,4> out{};raw.storeu(out.data());require(out==std::array<float,4>{1,2,3,4},"raw constructor lane order");
    canonical.storeu(out.data());require(out==std::array<float,4>{1,2,3,4},"mixed constructor lane order");
    auto a=::native::simd<float,4,FTZ_TEST_ARCH>{std::array<float,4>{5,6,7,8}};
    a.storeu(out.data());require(out==std::array<float,4>{5,6,7,8},"array constructor lane order");
    auto b=::native::load_simd<::native::simd<float,4,FTZ_TEST_ARCH>>(std::span<float,4>(out));b.storeu(out.data());require(out==std::array<float,4>{5,6,7,8},"span factory lane order");
#endif
  }
  template <std::size_t I> auto mixed_lane() {
    if constexpr(I==0)return ftz32(float(I+1));
    else return float(I+1);
  }
  template <class V> void check_memory() {
    using F = ::native::simd<float,V::lanes,FTZ_TEST_ARCH>;
    using R = ::native::simd<ftz32,V::lanes,FTZ_TEST_ARCH>;
    static_assert(std::same_as<typename F::register_type,V>);
    static_assert(std::same_as<typename R::register_type,V>);
    static_assert(sizeof(F)==sizeof(V) && sizeof(R)==sizeof(V));
    static_assert(std::is_trivially_copyable_v<F> && std::is_trivially_copyable_v<R>);
    static_assert(std::same_as<decltype(F(1)+ftz32(2)),R>);
    static_assert(std::same_as<decltype(ftz32(1)+F(2)),R>);
    static_assert(std::same_as<decltype(F(1)-ftz32(2)),R>);
    static_assert(std::same_as<decltype(ftz32(1)-F(2)),R>);
    static_assert(std::same_as<decltype(F(1)*ftz32(2)),R>);
    static_assert(std::same_as<decltype(ftz32(1)*F(2)),R>);
    static_assert(std::same_as<decltype(F(1)/ftz32(2)),R>);
    static_assert(std::same_as<decltype(ftz32(1)/F(2)),R>);
    static_assert(std::same_as<decltype(fma(F(1),ftz32(2),F(3))),R>);
    static_assert(std::same_as<decltype(fma(ftz32(1),F(2),F(3))),R>);
    static_assert(std::same_as<decltype(fma(F(1),F(2),ftz32(3))),R>);
    static_assert(std::same_as<decltype(fma(F(1),ftz32(2),ftz32(3))),R>);
    static_assert(std::same_as<decltype(fma(ftz32(1),F(2),ftz32(3))),R>);
    static_assert(std::same_as<decltype(fma(ftz32(1),ftz32(2),F(3))),R>);
    static_assert(std::same_as<decltype(F(1)<ftz32(2)),typename R::mask>);
    static_assert(std::same_as<decltype(ftz32(1)>=F(2)),typename R::mask>);
    std::array<std::uint32_t,V::lanes> promoted{};promoted.fill(0x80000000u);
    auto tiny=F::from_bits(0x80000001u);
    expected(tiny*ftz32(2),promoted,"raw scalar FTZ product promotion");
    expected(ftz32(2)*tiny,promoted,"scalar raw FTZ product promotion");
    promoted.fill(0u);
    expected(fma(tiny,ftz32(2),F(0)),promoted,"raw scalar FMA promotion");
    static_assert(std::same_as<decltype(fma(R(1),ftz32(2),F(3))),R>);
    static_assert(std::same_as<decltype(fma(F(1),R(2),ftz32(3))),R>);
    static_assert(std::same_as<decltype(fma(ftz32(1),F(2),R(3))),R>);
    static_assert(std::same_as<decltype(std::declval<F &>()+=R(0)),F &>);
    static_assert(std::same_as<decltype(std::declval<F &>()*=ftz32(0)),F &>);
    auto compound=F::from_bits(1u);compound+=R(0);expected(compound,promoted,"raw FTZ vector +=");
    compound=F::from_bits(1u);compound-=R(0);expected(compound,promoted,"raw FTZ vector -=");
    compound=F::from_bits(1u);compound*=R(1);expected(compound,promoted,"raw FTZ vector *=");
    compound=F::from_bits(1u);compound/=R(1);expected(compound,promoted,"raw FTZ vector /=");
    compound=F::from_bits(1u);compound+=ftz32(0);expected(compound,promoted,"raw FTZ scalar +=");
    compound=F::from_bits(1u);compound-=ftz32(0);expected(compound,promoted,"raw FTZ scalar -=");
    compound=F::from_bits(1u);compound*=ftz32(1);expected(compound,promoted,"raw FTZ scalar *=");
    compound=F::from_bits(1u);compound/=ftz32(1);expected(compound,promoted,"raw FTZ scalar /=");


    auto per_lane=[]<std::size_t... I>(std::index_sequence<I...>) {return ::native::simd<float,sizeof...(I),FTZ_TEST_ARCH>{float(I+1)...};}(std::make_index_sequence<V::lanes>{});
    auto mixed=[]<std::size_t... I>(std::index_sequence<I...>) {return ::native::simd<ftz32,sizeof...(I),FTZ_TEST_ARCH>{mixed_lane<I>()...};}(std::make_index_sequence<V::lanes>{});
    static_assert(std::same_as<decltype(per_lane),F> && std::same_as<decltype(mixed),R>);
    std::array<std::uint32_t,V::lanes> lane_words{};
    for(std::size_t i=0;i<V::lanes;++i)lane_words[i]=std::bit_cast<std::uint32_t>(float(i+1));
    expected(per_lane,lane_words,"all-width raw constructor lanes");expected(mixed,lane_words,"all-width mixed constructor lanes");
    alignas(64) std::array<float,V::lanes> a{},b{};
    for(std::size_t i=0;i<V::lanes;++i) a[i]=float(i+1);
    F::load(a.data()).store(b.data()); require(a==b,"aligned raw load/store");
    R::load(a.data()).store(b.data()); require(a==b,"aligned canonical load/store");
    std::array<float,V::lanes+2> u{},v{};
    for(std::size_t i=0;i<V::lanes;++i) u[i+1]=a[i];
    F::loadu(u.data()+1).storeu(v.data()+1); require(u==v,"unaligned raw load/store");
    R::loadu(u.data()+1).storeu(v.data()+1); require(u==v,"unaligned canonical load/store");
    for(std::size_t n=0;n<=V::lanes;++n) {
      v.fill(-123.0f);F::load_partial(u.data()+1,n).store_partial(v.data()+1,n);
      require(v[0]==-123.0f&&v[n+1]==-123.0f,"raw partial guards");
      for(std::size_t i=0;i<n;++i)require(v[i+1]==u[i+1],"raw partial payload");
      v.fill(-123.0f);R::load_partial(u.data()+1,n).store_partial(v.data()+1,n);
      require(v[0]==-123.0f&&v[n+1]==-123.0f,"canonical partial guards");
      for(std::size_t i=0;i<n;++i)require(v[i+1]==u[i+1],"canonical partial payload");
    }
    auto aligned=::native::load_simd<::native::simd<float,V::lanes,FTZ_TEST_ARCH>>(a.data(),::native::simd_memory<64>{});
    ::native::store_simd(b.data(),aligned,::native::simd_memory<64>{});require(a==b,"aligned helper");
    ::native::store_simd(v.data()+1,::native::load_simd<::native::simd<float,V::lanes,FTZ_TEST_ARCH>>(u.data()+1),::native::simd_memory<1,::native::simd_access::streaming>{});
    for(std::size_t i=0;i<V::lanes;++i)require(v[i+1]==u[i+1],"streaming hint ordinary fallback");
    static_assert(!::native::simd_memory<64,::native::simd_access::streaming>::non_temporal);
    std::array<ftz32,V::lanes+2> typed{},saved{};
    for(std::size_t i=0;i<V::lanes;++i)typed[i+1]=ftz32(float(i+1));
    auto typed_value=::native::load_simd<::native::simd<ftz32,V::lanes,FTZ_TEST_ARCH>>(typed.data()+1);
    ::native::store_simd(saved.data()+1,typed_value);
    for(std::size_t i=0;i<V::lanes;++i)require(saved[i+1].to_bits()==typed[i+1].to_bits(),"typed FTZ import/store");
    for(std::size_t count=0;count<=V::lanes;++count) {
      v.fill(-123);auto part=::native::load_simd_partial<::native::simd<ftz32,V::lanes,FTZ_TEST_ARCH>>(u.data()+1,count);
      ::native::store_simd_partial(v.data()+1,part,count);require(v.front()==-123&&v[count+1]==-123,"helper float tail guards");
      saved.fill(ftz32(-123));auto tp=::native::load_simd_partial<::native::simd<ftz32,V::lanes,FTZ_TEST_ARCH>>(typed.data()+1,count);
      ::native::store_simd_partial(saved.data()+1,tp,count);require(saved.front()==ftz32(-123)&&saved[count+1]==ftz32(-123),"helper typed tail guards");
      for(std::size_t i=0;i<count;++i)require(saved[i+1]==typed[i+1]&&v[i+1]==u[i+1],"helper tail payload");
    }
    ::native::store_simd_partial(static_cast<float*>(nullptr),::native::load_simd_partial<::native::simd<ftz32,V::lanes,FTZ_TEST_ARCH>>(static_cast<ftz32 const*>(nullptr),0),0);
    auto bad=F::from_bits(0x80000001u);::native::store_simd(typed.data()+1,bad);
    for(std::size_t i=0;i<V::lanes;++i)require(typed[i+1].to_bits()==0x80000000u,"raw-to-typed store canonicalizes");
    auto f=F::load(a.data());auto result=fma(f,2.0f,1.0f)+f*f;
    result.store(b.data());for(std::size_t i=0;i<V::lanes;++i)require(b[i]==a[i]*2+1+a[i]*a[i],"raw arithmetic");
    std::array<std::uint32_t,V::lanes> bits{},actual{};bits.fill(0x80000001u);
    F::load_bits(bits.data()).store_bits(actual.data());require(actual==bits,"raw import preserves subnormal");
    R::load_bits(bits.data()).store_bits(actual.data());for(auto x:actual)require(x==0x80000000u,"canonical import flushes subnormal");
  }
  // Two normals separated by k units of 2^-149 exercise exact cancellation,
  // every selected tiny-result sign, and both sides of minimum normal. Expected
  // bits follow from the integer difference, independently of the math core.
  template <class V> void check_add_boundaries() {
    using R = ::native::simd<ftz32,V::lanes,FTZ_TEST_ARCH>;
    std::vector<std::uint32_t> offsets;
    for (std::uint32_t k=0;k<=4096;++k) {
      offsets.push_back(k); offsets.push_back(0x00800000u-k);
    }
    std::uint32_t state=0x972bb941u;
    for (unsigned i=0;i<16385;++i) {
      state^=state<<13;state^=state>>17;state^=state<<5;
      offsets.push_back(state&0x007fffffu);
    }
    for (std::size_t start=0;start<offsets.size();start+=R::lanes) {
      std::array<std::uint32_t,R::lanes> a{},b{},negative{},positive{};
      for (std::size_t lane=0;lane<R::lanes;++lane) {
        auto k=start+lane<offsets.size()?offsets[start+lane]:0u;
        a[lane]=0x00800000u;b[lane]=0x00800000u+k;
        positive[lane]=k==0x00800000u?0x00800000u:0u;
        negative[lane]=k==0u?0u:positive[lane]|0x80000000u;
      }
      auto x=R::load_bits(a.data()),y=R::load_bits(b.data());
      expected(x-y,negative,"tiny subtraction negative");
      expected(y-x,positive,"tiny subtraction positive");
      expected(x+(-y),negative,"tiny addition negative");
      expected(y+(-x),positive,"tiny addition positive");
      auto wx=::native::broadcast<R,3>(x),wy=::native::broadcast<R,3>(y);
      auto sum=wx+(-wy),difference=wy-wx;
      for (std::size_t i=0;i<3;++i) {
        expected(sum.registers[i],negative,"wide tiny addition");
        expected(difference.registers[i],positive,"wide tiny subtraction");
      }
    }
  }
  template <class V> std::size_t check() {
    using R = ::native::simd<ftz32,V::lanes,FTZ_TEST_ARCH>;
    static_assert(sizeof(R) == sizeof(V));
    static_assert(std::is_convertible_v<float,R> && std::is_convertible_v<V,R> && std::is_convertible_v<R,V>);
    static_assert(std::same_as<decltype(R{}+1.0f),R> && std::same_as<decltype(1.0f+R{}),R>);
    static_assert(std::same_as<decltype(R{}+V{}),R> && std::same_as<decltype(V{}+R{}),R>);
    static_assert(std::same_as<decltype(fma(R{},1.0f,V{})),R>);
    static_assert(std::same_as<decltype(fma(V{},R{},1.0f)),R>);
    static_assert(std::same_as<decltype(fma(1.0f,V{},R{})),R>);
    static_assert(std::same_as<decltype(R{}+ftz32{}),R>);
    static_assert(std::same_as<decltype(ftz32{}+R{}),R>);
    std::array<std::uint32_t,R::lanes> unsafe_words{};unsafe_words.fill(0x3f800001u);
    expected(R::unsafe_from_float32(V(std::bit_cast<float>(0x3f800001u))),unsafe_words,"unsafe native factory");
    expected(R::unsafe_from_float32(std::bit_cast<float>(0x3f800001u)),unsafe_words,"unsafe broadcast factory");
    auto rows = bank();
    for (std::size_t start = 0; start < rows.size(); start += R::lanes) {
      std::array<std::uint32_t,R::lanes> a{},b{},c{},want{};
      for (std::size_t j = 0; j < R::lanes && start+j < rows.size(); ++j) {
        a[j]=rows[start+j][0];b[j]=rows[start+j][1];c[j]=rows[start+j][2];
      }
      R x=R::load_bits(a.data()),y=R::load_bits(b.data()),z=R::load_bits(c.data());
      for (std::size_t j=0;j<R::lanes;++j) {a[j]=core::ftz32_canonical(a[j]);b[j]=core::ftz32_canonical(b[j]);c[j]=core::ftz32_canonical(c[j]);}
      expected(x,a,"import");
      auto binary = [&](R result, auto op, char const * label) {
        for(std::size_t j=0;j<R::lanes;++j)want[j]=op(a[j],b[j]);
        expected(result,want,label);
      };
      binary(x+y,core::ftz32_add<>,"add");binary(x-y,core::ftz32_sub<>,"sub");
      binary(x*y,core::ftz32_mul,"mul");binary(x/y,core::ftz32_div<>,"div");
      for(std::size_t j=0;j<R::lanes;++j)want[j]=core::ftz32_fma(a[j],b[j],c[j]);
      expected(fma(x,y,z),want,"fma");
      for(std::size_t j=0;j<R::lanes;++j)want[j]=core::ftz32_sqrt(a[j]);
      expected(sqrt(x),want,"sqrt");
      for(std::size_t j=0;j<R::lanes;++j)want[j]=core::ftz32_neg(a[j]);
      expected(-x,want,"neg");
      for(std::size_t j=0;j<R::lanes;++j)want[j]=core::ftz32_abs(a[j]);
      expected(abs(x),want,"abs");
      for(std::size_t j=0;j<R::lanes;++j)want[j]=core::ftz32_less(a[j],b[j])?0x3f800000u:0;
      expected(select(x<y,R(1),R(0)),want,"less");
      for(std::size_t j=0;j<R::lanes;++j)want[j]=core::ftz32_equal(a[j],b[j])?0x3f800000u:0;
      expected(select(x==y,R(1),R(0)),want,"equal");
    }
    for (std::size_t count=0;count<=R::lanes;++count) {
      std::array<std::uint32_t,R::lanes+2> in{},out{};
      in.fill(0x00800001u);out.fill(0xdeadbeefu);
      R value=R::load_bits_partial(in.data()+1,count,0x80000001u);
      value.store_bits_partial(out.data()+1,count);
      require(out.front()==0xdeadbeefu && out[count+1]==0xdeadbeefu,"partial store guard");
      for(std::size_t j=0;j<count;++j)require(out[j+1]==in[j+1],"partial payload");
    }
    ::native::wide<R,3> a{{R(1),R(2),R(3)}},b{{R(2),R(3),R(4)}};
    auto result=fma(a,b,a)+1.0f;
    for(std::size_t j=0;j<3;++j) {
      std::array<std::uint32_t,R::lanes> want;want.fill(std::bit_cast<std::uint32_t>(float((j+1)*(j+3)+1)));
      expected(result.registers[j],want,"wide");
    }
    check_add_boundaries<V>();
    return rows.size();
  }
}
int main(int argc,char ** argv) {
  {
    require(argc==2,"gradual|flush");
    std::string mode=argv[1];require(mode=="gradual"||mode=="flush","mode");
    auto before=ftz::read_native_fp_state();std::size_t rows=0;
    bool const expected_admission=!FTZ_FP32_HARDWARE_FTZ || mode=="flush";
    {
      ftz::native_fp32_scope scope(mode=="flush"?ftz::native_fp32_mode::flush:ftz::native_fp32_mode::gradual);
      require(probe_ftz32_cpu().admitted()==expected_admission,"compiled arithmetic profile admission");
      if (expected_admission) {
      check_construction();
      rows=check<::native::simd<float,1,FTZ_TEST_ARCH>>();
      check_memory<::native::simd<float,1,FTZ_TEST_ARCH>>();
#if defined(__AVX2__) || defined(__ARM_NEON)
      require(check<::native::simd<float,4,FTZ_TEST_ARCH>>()==rows,"four row count"); check_memory<::native::simd<float,4,FTZ_TEST_ARCH>>();
#endif
#if defined(__AVX2__)
      require(check<::native::simd<float,8,FTZ_TEST_ARCH>>()==rows,"eight row count"); check_memory<::native::simd<float,8,FTZ_TEST_ARCH>>();
#endif
#if defined(__AVX512F__) && defined(__AVX512DQ__)
      require(check<::native::simd<float,16,FTZ_TEST_ARCH>>()==rows,"sixteen row count"); check_memory<::native::simd<float,16,FTZ_TEST_ARCH>>();
#endif
      if(FTZ_FP32_HARDWARE_FTZ==0 || mode=="flush") {
        check_functions<::native::simd<float,1,FTZ_TEST_ARCH>>();
#if defined(__AVX2__) || defined(__ARM_NEON)
        check_functions<::native::simd<float,4,FTZ_TEST_ARCH>>();
#endif
#if defined(__AVX2__)
        check_functions<::native::simd<float,8,FTZ_TEST_ARCH>>();
#endif
#if defined(__AVX512F__) && defined(__AVX512DQ__)
        check_functions<::native::simd<float,16,FTZ_TEST_ARCH>>();
#endif
      }
      }
      require(scope.controls_match(),"environment controls changed");
    }
    require(ftz::read_native_fp_state()==before,"caller environment not restored");
    std::cout<<"{\"passed\":true,\"mode\":\""<<mode<<"\",\"hardware_ftz\":"<<FTZ_FP32_HARDWARE_FTZ
             <<",\"rows\":"<<rows<<",\"native_lanes\":"<<(FTZ_TEST_PROFILE/32)
             <<",\"admission_negative_control\":"<<(!expected_admission?"true":"false")
             <<",\"transcendentals_checked\":"<<((FTZ_FP32_HARDWARE_FTZ==0 || mode=="flush")?"true":"false")
             <<",\"environment_restored\":true}\n";
    return 0;
  }
}

/**
 * \file
 * \license
 * SPDX-FileType: SOURCE
 * SPDX-FileCopyrightText: 2026 Edward Kmett <ekmett@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
 * \endlicense
 * \author Edward Kmett <ekmett@gmail.com>
 * \brief Checks vector FTZ arithmetic, import, implicit conversions and tails.
 */
