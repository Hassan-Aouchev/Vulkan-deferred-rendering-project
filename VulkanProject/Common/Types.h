#define BIT(x) (1u << (x))

#define ENUM_CLASS_FLAGS(EnumType) \
    inline constexpr EnumType operator|(EnumType lhs, EnumType rhs) noexcept { \
        return static_cast<EnumType>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)); \
    } \
    inline constexpr EnumType operator&(EnumType lhs, EnumType rhs) noexcept { \
        return static_cast<EnumType>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs)); \
    } \
    inline constexpr EnumType& operator|=(EnumType& lhs, EnumType rhs) noexcept { \
        return lhs = lhs | rhs; \
    }

template<typename EnumType>
constexpr bool EnumHasFlag(EnumType value, EnumType flag) noexcept {
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) == static_cast<uint32_t>(flag);
}