
#include "math/seq_math.hpp"
#include "math/vec.hpp"
#include "str_util.hpp"
#include "gui/container/container.hpp"
#include "gui/controls/inputfield.hpp"


template<typename T>
class gui_float_editor_numberinput_field : public gui_numberinput_field_generic<T> {
protected:
public:
    explicit gui_float_editor_numberinput_field(T* const _number) : gui_numberinput_field_generic<T>(_number) {
    }
    T parseLiteral(const char* szNumber) override = 0;
    String valueToStringLiteral(T val) override   = 0;

private:
    void setValue(T newVal) override {
        *this->number = newVal;
    }
};
class float_repr_textfield_hex : public gui_float_editor_numberinput_field<float> {
public:
    explicit float_repr_textfield_hex(float* const _ptrF) : gui_float_editor_numberinput_field<float>(_ptrF) {
    }
    float parseLiteral(const char* szNumber) override {
        String input          = String(szNumber);
        input                 = StringTrim(input);
        String::size_type pos = input.find("0x");
        if (pos == 0) {
            input = input.substr(2U);
        }
        uint64_t number        = strtoll(StringAsCStr(input), nullptr, 16U);
        uint32_t number_u32    = static_cast<uint32_t>(number & 0xFFFFFFFFU);
        float number_float_rep = *reinterpret_cast<float*>(&number_u32);
        return number_float_rep;
    }
    String valueToStringLiteral(float val) override {
        int32_t float_as_s32 = *reinterpret_cast<int32_t*>(&val);
        return StringFormat("0x%08X", float_as_s32);
    }
};
class float_repr_textfield_binary : public gui_float_editor_numberinput_field<float> {
public:
    explicit float_repr_textfield_binary(float* const _ptrF) : gui_float_editor_numberinput_field<float>(_ptrF) {
    }
    float parseLiteral(const char* szNumber) override {
        String input = String(szNumber);
        input        = StringTrim(input);
        if (input.find("0b") == 0) {
            input = input.substr(2U);
        }
        uint32_t number_u32 = 0;
        for (uint8_t bit = 0; bit < input.length() && bit < 32U; ++bit) {
            auto charBit = input.at(input.length() - 1U - bit);
            if (charBit != '0' && charBit != '1') {
                return 0.0f;
            }
            uint8_t bitSet = ('1' == charBit) & 1U;
            number_u32 |= bitSet << bit;
        }
        float number_float_rep = *reinterpret_cast<float*>(&number_u32);
        return number_float_rep;
        // return atof(szNumber);
    }
    String valueToStringLiteral(float val) override {
        int32_t float_as_s32 = *reinterpret_cast<int32_t*>(&val);
        return FormatBinaryString(float_as_s32);
    }
};
class float_repr_textfield_dec_long : public gui_float_editor_numberinput_field<float> {
public:
    explicit float_repr_textfield_dec_long(float* const _ptrF) : gui_float_editor_numberinput_field<float>(_ptrF) {
    }
    float parseLiteral(const char* szNumber) override {
        return float(atof(szNumber));
    }
    String valueToStringLiteral(float val) override {
        return StringFormat("%0.32f", val);
    }
};
class float_repr_textfield_dec_short : public gui_float_editor_numberinput_field<float> {
public:
    explicit float_repr_textfield_dec_short(float* const _ptrF) : gui_float_editor_numberinput_field<float>(_ptrF) {
    }
    float parseLiteral(const char* szNumber) override {
        return float(atof(szNumber));
    }
    String valueToStringLiteral(float val) override {
        return StringFormat("%.32e", val);
    }
};
class float_repr_textfield_dec_signed : public gui_float_editor_numberinput_field<int32_t> {
public:
    explicit float_repr_textfield_dec_signed(int32_t* const _ptrF) : gui_float_editor_numberinput_field<int32_t>(_ptrF) {
    }
    int32_t parseLiteral(const char* szNumber) override {
        return math::floordS32(atof(szNumber));
    }
    String valueToStringLiteral(int32_t val) override {
        return StringFormat("%d", val);
    }
};

template<typename T>
constexpr uint8_t getTypeExponentSize() {
    return 0U;
};
template<typename T>
constexpr uint8_t getTypeManitssaSize();
template<typename T>
constexpr uint32_t getTypeManitssaMask();
template<typename T>
constexpr uint8_t getTypeExponentMask();
template<typename T>
constexpr uint16_t getTypeExponentBias();
template<typename T>
constexpr uint8_t getTypeSignBit() {
    return 0U;
}
template<typename T>
constexpr uint8_t getSignBit(T f) {
    return 0U;
}
template<typename T>
uint32_t getMantissaBits(T f) {
    return 0U;
}
template<typename T>
constexpr uint8_t getTypeSizeBits() {
    return 0U;
};
template<>
constexpr uint8_t getTypeExponentSize<float>() {
    return 8U;
}
template<>
constexpr uint8_t getTypeManitssaSize<float>() {
    return 23U;
}
template<>
constexpr uint32_t getTypeManitssaMask<float>() {
    return (1U << getTypeManitssaSize<float>()) - 1U;
}
template<>
constexpr uint8_t getTypeExponentMask<float>() {
    return (1U << getTypeExponentSize<float>()) - 1U;
}
template<>
constexpr uint16_t getTypeExponentBias<float>() {
    return 127;
}
template<>
constexpr uint8_t getTypeSizeBits<float>() {
    return sizeof(float) * 8U;
}
template<>
constexpr uint8_t getTypeSignBit<float>() {
    return getTypeSizeBits<float>() - 1U;
}
template<>
uint8_t getSignBit(float f) {
    return ((*reinterpret_cast<uint32_t*>(&f)) >> getTypeSignBit<float>()) & 1U;
}
template<>
uint32_t getMantissaBits(float f) {
    return (*reinterpret_cast<uint32_t*>(&f)) & getTypeManitssaMask<float>();
}

namespace {
template<typename T>
T getFloatMantissa(T f) {
    uint32_t mnts_u32 = getMantissaBits(f);
    T fMantissa       = 1.0;
    for (uint8_t bit = 0; bit < getTypeManitssaSize<T>(); bit++) {
        uint32_t bitSet = (mnts_u32 >> (getTypeManitssaSize<T>() - 1U - bit)) & 1U;
        fMantissa += (float) bitSet / (1U << (bit + 1U));
    }
    return fMantissa;
}
template<typename T>
uint32_t getExponentBits(T f) {
    uint32_t float_as_u32  = *reinterpret_cast<uint32_t*>(&f);
    uint32_t float_shifted = (float_as_u32 >> getTypeManitssaSize<T>());
    return float_shifted & getTypeExponentMask<T>();
}
template<typename T>
int32_t getExponentBitsSigned(T f) {
    uint32_t float_as_u32 = *reinterpret_cast<uint32_t*>(&f);
    uint32_t exponent_u32 = (float_as_u32 >> getTypeManitssaSize<T>()) & getTypeExponentMask<T>();
    //TODO: this needs to be handled left aligned to work with 11 bit exponent sign
    int8_t exponent_s32 = *reinterpret_cast<int8_t*>(&exponent_u32);
    return exponent_s32;
}
}
template<typename T>
class float_repr_textfield_exp_dec_unsigned : public gui_float_editor_numberinput_field<uint32_t> {
    uint32_t edit = 0;
    T* const number;
    bool hexFmt = false;

public:
    explicit float_repr_textfield_exp_dec_unsigned(T* const _ptrF) : gui_float_editor_numberinput_field<uint32_t>(&edit), number(_ptrF) {
    }
    void setHexFmt(bool b) {
        hexFmt = b;
    }
    virtual uint32_t getValue() override {
        uint32_t float_as_u32 = *reinterpret_cast<uint32_t*>(number);
        uint32_t exponentBits = (float_as_u32 >> getTypeManitssaSize<T>()) & getTypeExponentMask<T>();
        edit                  = exponentBits & getTypeExponentMask<T>();
        return edit;
    }
    void setValue(uint32_t newVal) override {
        edit                     = newVal;
        uint64_t newExponentBits = (newVal & getTypeExponentMask<T>()) << getTypeManitssaSize<T>();
        uint32_t float_as_u32    = *reinterpret_cast<uint32_t*>(number);
        float_as_u32 &= ~(getTypeExponentMask<T>() << getTypeManitssaSize<T>());
        float_as_u32 |= newExponentBits;
        *reinterpret_cast<uint32_t*>(number) = float_as_u32;
    }
    uint32_t parseLiteral(const char* szNumber) override {
        String strInput       = StringTrim(String(szNumber));
        String::size_type pos = strInput.find("0x");
        uint8_t base          = hexFmt ? 16 : 10;
        if (pos == 0) {
            strInput = strInput.substr(2U);
            base     = 16;
        }
        uint32_t float_as_u32 = *reinterpret_cast<uint32_t*>(number);
        uint64_t input_u64    = strtoll(StringAsCStr(strInput), nullptr, base);

        uint64_t newExponentBits = (input_u64 & getTypeExponentMask<T>()) << getTypeManitssaSize<T>();
        float_as_u32 &= ~(getTypeExponentMask<T>() << getTypeManitssaSize<T>());
        float_as_u32 |= newExponentBits;
        *reinterpret_cast<uint32_t*>(number) = float_as_u32;
        return (float_as_u32 >> getTypeManitssaSize<T>()) & getTypeExponentMask<T>();
    }
    String valueToStringLiteral(uint32_t val) override {
        return StringFormat(hexFmt ? "0x%02X" : "%d", val);
    }
};
template<typename T>
class float_repr_textfield_mantissa_dec_unsigned : public gui_float_editor_numberinput_field<uint32_t> {
    uint32_t edit = 0;
    T* const number;
    bool hexFmt = false;

public:
    explicit float_repr_textfield_mantissa_dec_unsigned(T* const _ptrF) : gui_float_editor_numberinput_field<uint32_t>(&edit), number(_ptrF) {
    }
    void setHexFmt(bool b) {
        hexFmt = b;
    }
    virtual uint32_t getValue() override {
        uint32_t float_as_u32 = *reinterpret_cast<uint32_t*>(number);
        uint32_t mantissaBits = float_as_u32 & getTypeManitssaMask<T>();
        edit                  = mantissaBits;
        return edit;
    }
    void setValue(uint32_t newVal) override {
        edit                     = newVal;
        uint64_t newMantissaBits = newVal & getTypeManitssaMask<T>();
        uint32_t float_as_u32    = *reinterpret_cast<uint32_t*>(number);
        float_as_u32 &= ~getTypeManitssaMask<T>();
        float_as_u32 |= newMantissaBits;
        *reinterpret_cast<uint32_t*>(number) = float_as_u32;
    }
    uint32_t parseLiteral(const char* szNumber) override {
        String strInput       = StringTrim(String(szNumber));
        String::size_type pos = strInput.find("0x");
        uint8_t base          = hexFmt ? 16 : 10;
        if (pos == 0) {
            strInput = strInput.substr(2U);
            base     = 16;
        }
        uint32_t float_as_u32 = *reinterpret_cast<uint32_t*>(number);
        uint64_t input_u64    = strtoll(StringAsCStr(strInput), nullptr, base);


        uint64_t newMantissaBits = (input_u64 & getTypeManitssaMask<T>());
        float_as_u32 &= ~getTypeManitssaMask<T>();
        float_as_u32 |= newMantissaBits;
        *reinterpret_cast<uint32_t*>(number) = float_as_u32;
        return (float_as_u32 & getTypeManitssaMask<T>());
    }
    String valueToStringLiteral(uint32_t val) override {
        return StringFormat(hexFmt ? "0x%08X" : "%d", val);
    }
};


template<typename T>
class guictr_edit_ieee_float : public guictr_base {


    static constexpr int INSET_OUTER = 8;
    static constexpr int INSET_INNER = 24;
    int fontSize                     = 0;
    ivec2 sizeView{ 0, 0 };
    ivec2 sizeViewInner{ 0, 0 };
    vec2 aspectView{ 0.0f, 0.0f };
    ivec2 lastContentSize{ 0, 0 };


    const float grpInset = 6;
    float heightGrp      = 0;


    /**
     * grp 0 = sign
     * grp 1 = exponent
     * grp 2 = mantiass
     */
    static constexpr uint8_t NUM_GRPS                    = 3U;
    static constexpr const uint8_t numBitsInGrp[3]       = { 1U, getTypeExponentSize<T>(), getTypeManitssaSize<T>() };
    static constexpr const uint8_t grpBitStart[]         = { getTypeSignBit<T>(), getTypeManitssaSize<T>(), 0U };
    static constexpr const uint8_t grpBitEnd[]           = { static_cast<uint8_t>(getTypeSignBit<T>() + 1U), grpBitStart[0], grpBitStart[1] };
    static constexpr const char* strGroupNames[NUM_GRPS] = {
        "Sign",
        "Exponent",
        "Mantissa",
    };
    static constexpr uint32_t colorGrp[NUM_GRPS] = { 0xffd2d2e7, 0xffc0ddc2, 0xffddd0c4 };
    int32_t textSize = 32;

    float xGrps         = 0.0f;
    float yGrps         = 0.0f;
    float yBit          = 0;
    float yText         = 0;
    float yTextVal      = 0;
    float yTextEnc      = 0;
    float yFields       = 0;
    float yTextFloatBig = 0;

    float number = 0.0f;
    struct bit_hit_t {
        int32_t idx;
        vec2 hitOffsetRelative;
    };
    bit_hit_t bitHit;
    float_repr_textfield_binary textFld_floatRepr_binary;
    float_repr_textfield_hex textFld_floatRepr_hex;
    float_repr_textfield_dec_long textFld_floatRepr_decLong;
    float_repr_textfield_dec_short textFld_floatRepr_decShort;
    float_repr_textfield_exp_dec_unsigned<T> textFld_floatExponent_unsigned;
    float_repr_textfield_mantissa_dec_unsigned<T> textFld_floatMantissa_unsigned;
    float_repr_textfield_exp_dec_unsigned<T> textFld_floatExponent_unsignedH;
    float_repr_textfield_mantissa_dec_unsigned<T> textFld_floatMantissa_unsignedH;
    std::vector<gui_numberinput_field_base*> textFields;

public:
    guictr_edit_ieee_float() : guictr_base(),
                               textFld_floatRepr_binary(&number),
                               textFld_floatRepr_hex(&number),
                               textFld_floatRepr_decLong(&number),
                               textFld_floatRepr_decShort(&number),
                               textFld_floatExponent_unsigned(&number),
                               textFld_floatMantissa_unsigned(&number),
                               textFld_floatExponent_unsignedH(&number),
                               textFld_floatMantissa_unsignedH(&number) {
        textFields.push_back(&textFld_floatRepr_decShort);
        textFields.push_back(&textFld_floatRepr_decLong);
        textFields.push_back(&textFld_floatRepr_hex);
        textFields.push_back(&textFld_floatRepr_binary);
        add(&textFld_floatExponent_unsigned);
        add(&textFld_floatMantissa_unsigned);
        add(&textFld_floatExponent_unsignedH);
        add(&textFld_floatMantissa_unsignedH);
        textFld_floatExponent_unsignedH.setHexFmt(true);
        textFld_floatMantissa_unsignedH.setHexFmt(true);
        for (auto* pTextfield : textFields) {
            add(pTextfield);
        }
        setCanMouseHit(true);
    }
    ~guictr_edit_ieee_float() {
        removeGuis();
    }

    bool handleKeyInput(KeyEvent& kevt) override {
        if (kevt.type != KeyboardState::K_RELEASE) {
            if (kevt.keyCode == KeyboardKey::DAW_KB_SPACE) {
                return true;
            }
        }
        return false;
    }
    void buttonClicked(guibase* button) override {
    }
    void layout() override {

        ivec2 cs               = getSizeContent();
        sizeView               = cs - INSET_OUTER * 2;
        sizeViewInner          = sizeView - INSET_INNER * 2;
        heightGrp              = math::max((textSize + grpInset) * 7.0f, sizeViewInner.y / 2.0f - grpInset * 2.0f);
        float aspectX          = sizeViewInner.x / (float) sizeViewInner.y;
        aspectView             = vec2(aspectX < 1.0f ? 1.0f : sizeViewInner.y / (float) sizeViewInner.x, aspectX < 1.0f ? aspectX : 1.0f);
        float posXFields       = INSET_OUTER + INSET_INNER;
        float heightFields     = (sizeViewInner.y / 3.0f) / (textFields.size() + 1.0f);
        const float posYFields = INSET_OUTER + INSET_INNER + sizeViewInner.y - heightFields * (textFields.size() + 1);
        float posYFieldsOffset = posYFields + heightFields / 2.0f;
        float spacingYFields   = 5;
        for (auto pTextfield : textFields) {
            pTextfield->pos  = { math::floorfS32(posXFields), math::floorfS32(posYFieldsOffset + spacingYFields / 2.0f) };
            pTextfield->size = { math::floorfS32(sizeViewInner.x), math::floorfS32(heightFields - spacingYFields) };
            posYFieldsOffset += heightFields;
        }

        if (lastContentSize != cs) {
            lastContentSize = cs;
        }
        const float x = INSET_OUTER + INSET_INNER;
        //        const float y = INSET_OUTER + INSET_INNER + (sizeViewInner.y / 2) - heightGrp;
        const float widthBit = getWidthBit();
        //        const float bitRectWidth = math::floorfS32(math::max(1.0f, widthBit-4.0f));
        xGrps         = x;
        yGrps         = grpInset;
        yText         = yGrps + textSize / 2.0f;
        yTextVal      = yGrps + textSize + grpInset + textSize;
        yTextEnc      = yGrps + textSize + grpInset + textSize + grpInset + textSize;
        yFields       = yGrps + textSize + grpInset + textSize + grpInset + textSize + grpInset + textSize;
        yTextFloatBig = yGrps + heightGrp + (posYFields - (yGrps + heightGrp)) / 2;
        yBit          = yGrps + heightGrp - widthBit;
        for (auto* gui : guis) {
            gui->layout();
        }
    }
    float getWidthBit() {
        return math::max(32.0f, (sizeViewInner.x - (grpInset * 2) * 3) / static_cast<float>(getTypeSizeBits<T>()));
    }
    void render(NVGcontext* vg) override {

        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }

        assert(getTypeExponentMask<T>() == 255U);
        const int32_t float_as_s32 = *reinterpret_cast<int32_t*>(&this->number);

        const float widthBit     = getWidthBit();
        const float bitRectWidth = math::floorfS32(math::max(1.0f, widthBit - 4.0f));
        float grpsWidth          = 0.0f;
        float sizeXGrp[3];
        for (uint8_t i = 0U; i < 3U; i++) {
            sizeXGrp[i] = math::max(bitRectWidth + 40, numBitsInGrp[i] * widthBit) + grpInset * 2;
            grpsWidth += sizeXGrp[i];
        }
        float xOffset = xGrps;
        for (uint8_t grp = 0; grp < 3U; grp++) {
            float xPosGrp  = xOffset + grpInset;
            float widthGrp = sizeXGrp[grp] - grpInset * 2;
            nvgBeginPath(vg);
            nvgRect(vg, xPosGrp, yGrps, widthGrp, heightGrp);
            nvgFillColor(vg, rgbToNvg(colorGrp[grp]));
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgRect(vg, xPosGrp, yGrps, widthGrp, textSize);
            nvgFillColor(vg, rgbToNvg(0x7F7f7f7f));
            nvgFill(vg);
            xOffset += sizeXGrp[grp];
        }
        xOffset = xGrps;
        for (uint8_t grp = 0; grp < 3U; grp++) {
            float xPosCenter = xOffset + (sizeXGrp[grp] / 2.0f);
            uint8_t startBit = grpBitStart[grp];
            uint8_t endBit   = grpBitEnd[grp];
            float xBit       = xPosCenter - (endBit - startBit) * widthBit * 0.5f;
            for (uint8_t bit = endBit; bit > startBit; bit--) {
                ivec2 bitPos     = { xBit, yBit };
                uint8_t bitState = (float_as_s32 >> (bit - 1U)) & 1U;
                nvgBeginPath(vg);
                nvgRect(vg, bitPos.x + (widthBit - bitRectWidth) / 2, bitPos.y - bitRectWidth / 2, bitRectWidth, bitRectWidth);
                //                nvgRect(vg, bitPos.x, bitPos.y, widthBit, widthBit);
                nvgFillColor(vg, rgbToNvg(bitState ? 0xffaaaaaa : 0xff333333));


                nvgFill(vg);
                xBit += widthBit;
            }
            xOffset += sizeXGrp[grp];
        }

        xOffset = xGrps;
        for (uint8_t grp = 0; grp < 3U; grp++) {
            float xPosCenterText = xOffset + (sizeXGrp[grp] / 2.0f);
            setFont(vg, textSize, G_BLACK, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, xPosCenterText + 1, yText + 1, (strGroupNames[grp]), nullptr);
            nvgFillColor(vg, THEMECOL_TEXT);
            nvgText(vg, xPosCenterText, yText, (strGroupNames[grp]), nullptr);
            xOffset += sizeXGrp[grp];
        }

        xOffset = xGrps;
        for (uint8_t grp = 0; grp < 3U; grp++) {
            float xPosCenter = xOffset + (sizeXGrp[grp] / 2.0f);
            uint8_t startBit = grpBitStart[grp];
            uint8_t endBit   = grpBitEnd[grp];
            float xBit       = xPosCenter - (endBit - startBit) * widthBit * 0.5f;
            for (uint8_t bit = endBit; bit > startBit; bit--) {
                ivec2 bitPos     = { xBit, yBit };
                uint8_t bitState = (float_as_s32 >> (bit - 1U)) & 1U;
                setFont(vg, textSize, G_BLACK, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgText(vg, bitPos.x + widthBit / 2 + 1, bitPos.y + 1, (bitState) ? "1" : "0", nullptr);
                nvgFillColor(vg, THEMECOL_TEXT);
                nvgText(vg, bitPos.x + widthBit / 2, bitPos.y, (bitState) ? "1" : "0", nullptr);

                xBit += widthBit;
            }
            xOffset += sizeXGrp[grp];
        }
        xOffset = xGrps;
        setFont(vg, textSize, G_BLACK, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (uint8_t grp = 0; grp < 3U; grp++) {
            float xPosCenterText = xOffset + (sizeXGrp[grp] / 2.0f);
            float yPosText       = yTextVal;
            String str           = "";
            if (grp == 0) {
                str = getSignBit(this->number) == 0 ? "+1" : "-1";
            }
            if (grp == 1) {
                str = "2";
                nvgFillColor(vg, G_BLACK);
                nvgText(vg, xPosCenterText + 1, yPosText + 1, StringAsCStr(str), nullptr);
                nvgFillColor(vg, THEMECOL_TEXT);
                nvgText(vg, xPosCenterText, yPosText, StringAsCStr(str), nullptr);
                int32_t fpExponent = getExponentBits<T>(this->number) - getTypeExponentBias<T>();
                str                = StringFormat("%d", fpExponent);
                yPosText -= textSize / 2.0f;
                xPosCenterText += textSize;
            }
            if (grp == 2) {
                float fMantissa = getFloatMantissa<T>(this->number);
                str             = StringFormat("%f", fMantissa);
            }

            nvgFillColor(vg, G_BLACK);
            nvgText(vg, xPosCenterText + 1, yPosText + 1, StringAsCStr(str), nullptr);
            nvgFillColor(vg, THEMECOL_TEXT);
            nvgText(vg, xPosCenterText, yPosText, StringAsCStr(str), nullptr);

            xOffset += sizeXGrp[grp];
        }
        xOffset = xGrps;
        setFont(vg, textSize, G_BLACK, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        for (uint8_t grp = 0; grp < 3U; grp++) {
            float xPosCenterText = xOffset + (sizeXGrp[grp] / 2.0f);
            String str;
            if (grp == 0) {
                str = StringFormat("%d", getSignBit<T>(this->number));
            }
            if (grp == 1) {
                str          = StringFormat("%d", getExponentBits<T>(this->number));
                auto& fldExp = textFld_floatExponent_unsigned;
                fldExp.size  = { textSize * 2, textSize };
                fldExp.pos   = { xPosCenterText, yFields };
                fldExp.pos.x -= fldExp.size.x;
                fldExp.pos.x -= textSize / 2;
                fldExp.layout();
                auto& fldExp2 = textFld_floatExponent_unsignedH;
                fldExp2.size  = { textSize * 2, textSize };
                fldExp2.pos   = { xPosCenterText, yFields };
                fldExp2.pos.x += int32_t(textSize * 1.5f);
                fldExp2.layout();
                nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, (theme->getColor(GuiColor::COL_LABEL_INACTIVE)));
                nvgText(vg, fldExp.pos.x - textSize * 1.8f, fldExp.pos.y + fldExp.size.y / 2.0f, "Dec", nullptr);
                nvgText(vg, fldExp2.pos.x - textSize * 1.8f, fldExp2.pos.y + fldExp2.size.y / 2.0f, "Hex", nullptr);
            }
            if (grp == 2) {
                str          = StringFormat("%d", getMantissaBits<T>(this->number));
                auto& fldExp = textFld_floatMantissa_unsigned;
                fldExp.size  = { textSize * 5, textSize };
                fldExp.pos   = { xPosCenterText, yFields };
                fldExp.pos.x -= fldExp.size.x;
                fldExp.pos.x -= textSize / 2;
                fldExp.layout();
                auto& fldExpH = textFld_floatMantissa_unsignedH;
                fldExpH.size  = { textSize * 5, textSize };
                fldExpH.pos   = { xPosCenterText, yFields };
                fldExpH.pos.x += int32_t(textSize * 1.5f);
                fldExpH.layout();
                nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, (theme->getColor(GuiColor::COL_LABEL_INACTIVE)));
                nvgText(vg, fldExp.pos.x - textSize * 1.8f, fldExp.pos.y + fldExp.size.y / 2.0f, "Dec", nullptr);
                nvgText(vg, fldExpH.pos.x - textSize * 1.8f, fldExpH.pos.y + fldExpH.size.y / 2.0f, "Hex", nullptr);
            }
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, G_BLACK);
            nvgText(vg, xPosCenterText + 1, yTextEnc + 1, StringAsCStr(str), nullptr);
            nvgFillColor(vg, THEMECOL_TEXT);
            nvgText(vg, xPosCenterText, yTextEnc, StringAsCStr(str), nullptr);
            xOffset += sizeXGrp[grp];
        }
        float centerX      = xGrps + grpsWidth / 2.0f;
        float fMantissa    = getFloatMantissa(this->number);
        int32_t fpExponent = getExponentBits<T>(this->number) - getTypeExponentBias<T>();
        double dFloat      = fMantissa * pow((double) 2.0, (double) fpExponent);
        String str         = StringFormat("%0.64f", dFloat);
        auto fontSize      = textFields[0]->getField().fontSize();
        setFont(vg, fontSize, G_BLACK, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        //        nvgFillColor(vg, G_BLACK);
        nvgText(vg, centerX + 1, yTextFloatBig + 1, StringAsCStr(str), nullptr);
        nvgFillColor(vg, THEMECOL_TEXT);
        nvgText(vg, centerX, yTextFloatBig, StringAsCStr(str), nullptr);

        for (auto c : guis) {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        if (bitHit.idx > -1 && bitHit.idx < getTypeSizeBits<T>()) {
            uint32_t* float_as_u32_ptr = reinterpret_cast<uint32_t*>(&this->number);
            *float_as_u32_ptr ^= 1U << bitHit.idx;
            bitHit = { -1, vec2(0) };
        }
    }
    void handleDraggedMove(MouseEvent& evt) override {
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        bitHit = { -1, vec2(0) };
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos)) {
            ivec2 localMouse = this->toContainerSpace(mpos);

            const float widthBit     = getWidthBit();
            const float bitRectWidth = math::floorfS32(math::max(1.0f, widthBit - 4.0f));
            float sizeXGrp[3];
            for (uint8_t i = 0U; i < 3U; i++) {
                sizeXGrp[i] = math::max(bitRectWidth + 40, numBitsInGrp[i] * widthBit) + grpInset * 2;
            }
            for (guibase* gui : guis) {
                if (!gui->isVisible())
                    continue;
                if (gui->mouseHitTest(localMouse, evt)) {
                    return true;
                }
            }

            if (canMouseHit()) {
                bitHit        = { -1, vec2(0) };
                float xOffset = xGrps;
                for (uint8_t grp = 0; grp < 3U; grp++) {
                    float xPosCenter = xOffset + (sizeXGrp[grp] / 2.0f);
                    uint8_t startBit = grpBitStart[grp];
                    uint8_t endBit   = grpBitEnd[grp];
                    float xBit       = xPosCenter - (endBit - startBit) * widthBit * 0.5f;
                    for (uint8_t bit = endBit; bit > startBit; bit--) {
                        ivec2 bitPos = { xBit, yBit };
                        xBit += widthBit;
                        if (localMouse.x >= bitPos.x + (widthBit - bitRectWidth) / 2 && localMouse.x < (bitPos.x + (widthBit - bitRectWidth) / 2) + bitRectWidth) {
                            if (localMouse.y >= bitPos.y - bitRectWidth / 2 && localMouse.y < bitPos.y + bitRectWidth / 2) {
                                bitHit = bit_hit_t{ static_cast<int32_t>(bit) - 1, vec2(localMouse - ivec2(bitPos.x + (widthBit - bitRectWidth) / 2, bitPos.y - bitRectWidth / 2)) / vec2(bitRectWidth) };
                                evt.requestFocus(this);
                                return true;
                            }
                        }
                    }
                    xOffset += sizeXGrp[grp];
                }
                evt.requestFocus(this);
                return true;
            }
        }
        return false;
    }
};

guictr_base* makeFloatEditCtr() {
    return new guictr_edit_ieee_float<float>();
}
