#include "Product.h"

// VETUS: product info, factory bank and its own interface strings.
// Values: age / noise / wobble / mix 0..1, medium 0 TAPE 1 VINYL 2 SAMPLER 3 RADIO, gate_noise 0/1, dB.

namespace
{
const char* translations (const juce::String& code)
{
    if (code.startsWith ("es")) return R"X(
"From almost new to found in a basement" = "De casi nuevo a encontrado en un sótano"
"Hiss, crackle and static of the medium (middle = as it comes)" = "Soplido, crujidos y estática del medio (al medio = como viene)"
"Wow and flutter: the pitch movement of the medium (middle = as it comes)" = "Wow y flutter: el movimiento de afinación del medio (al medio = como viene)"
"The noise only plays while there is audio, so the end of the track is true silence" = "El ruido suena solo mientras hay audio, así el final del tema queda en silencio real"
"A lo-fi machine with four media: tape, vinyl, an early sampler and AM radio. Turn AGE." = "Una máquina lo-fi con cuatro medios: cinta, vinilo, un sampler de los 80 y radio AM. Girá AGE."
)X";
    if (code == "fr") return R"X(
"From almost new to found in a basement" = "De presque neuf à trouvé dans une cave"
"Hiss, crackle and static of the medium (middle = as it comes)" = "Souffle, craquements et parasites du support (au milieu = d'origine)"
"Wow and flutter: the pitch movement of the medium (middle = as it comes)" = "Pleurage et scintillement du support (au milieu = d'origine)"
"The noise only plays while there is audio, so the end of the track is true silence" = "Le bruit ne joue qu'avec l'audio : la fin du morceau reste un vrai silence"
"A lo-fi machine with four media: tape, vinyl, an early sampler and AM radio. Turn AGE." = "Une machine lo-fi à quatre supports : bande, vinyle, un vieux sampler et radio AM. Tournez AGE."
)X";
    if (code == "zh") return R"X(
"From almost new to found in a basement" = "从几乎全新到地下室里翻出来的"
"Hiss, crackle and static of the medium (middle = as it comes)" = "介质的嘶声、噼啪声和静电（中间 = 原样）"
"Wow and flutter: the pitch movement of the medium (middle = as it comes)" = "抖晃：介质的音高波动（中间 = 原样）"
"The noise only plays while there is audio, so the end of the track is true silence" = "只有在有音频时才出现噪声，结尾保持真正的静音"
"A lo-fi machine with four media: tape, vinyl, an early sampler and AM radio. Turn AGE." = "四种介质的 Lo-Fi 机器：磁带、黑胶、早期采样器和 AM 收音机。转动 AGE。"
)X";
    if (code == "ja") return R"X(
"From almost new to found in a basement" = "ほぼ新品から、地下室で見つけたものまで"
"Hiss, crackle and static of the medium (middle = as it comes)" = "メディアのヒス、クラックル、ノイズ（中央 = そのまま）"
"Wow and flutter: the pitch movement of the medium (middle = as it comes)" = "ワウ・フラッター：メディアのピッチの揺れ（中央 = そのまま）"
"The noise only plays while there is audio, so the end of the track is true silence" = "ノイズは音があるときだけ鳴り、曲の終わりは完全な無音に"
"A lo-fi machine with four media: tape, vinyl, an early sampler and AM radio. Turn AGE." = "テープ、レコード、初期サンプラー、AM ラジオの 4 つのメディアを持つローファイマシン。AGE を回すだけ。"
)X";
    return nullptr;
}
}

namespace nob
{

const ProductInfo& product()
{
    static const ProductInfo info { "VETUS", "LO-FI MACHINE  /  LITE",
                                    "A lo-fi machine with four media: tape, vinyl, an early sampler and AM radio. Turn AGE.",
                                    ".vetus", translations };
    return info;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> bank =
    {
        { "", "Init", "", {} },

        { "01 Tape", "Cassette Demo",      "TAPE", { { "age", 0.55f }, { "medium", 0 } } },
        { "01 Tape", "Warm Reel",          "TAPE", { { "age", 0.25f }, { "medium", 0 }, { "noise", 0.3f } } },
        { "01 Tape", "Chewed Tape",        "TAPE", { { "age", 0.95f }, { "medium", 0 }, { "wobble", 0.9f } } },

        { "02 Vinyl", "Lo-Fi Beats",       "VINYL", { { "age", 0.5f }, { "medium", 1 } } },
        { "02 Vinyl", "Dusty 45",          "VINYL", { { "age", 0.75f }, { "medium", 1 }, { "noise", 0.7f } } },
        { "02 Vinyl", "Clean Crackle Bed", "VINYL", { { "age", 0.3f }, { "medium", 1 }, { "noise", 0.8f }, { "wobble", 0.2f } } },

        { "03 Sampler", "12-Bit Drums",    "SAMPLER", { { "age", 0.35f }, { "medium", 2 } } },
        { "03 Sampler", "Crunchy Chops",   "SAMPLER", { { "age", 0.65f }, { "medium", 2 } } },
        { "03 Sampler", "8 kHz Grit",      "SAMPLER", { { "age", 1.0f }, { "medium", 2 }, { "mix", 0.7f } } },

        { "04 Radio", "AM Vocal",          "RADIO", { { "age", 0.4f }, { "medium", 3 } } },
        { "04 Radio", "Telephone Break",   "RADIO", { { "age", 0.8f }, { "medium", 3 }, { "noise", 0.3f } } },
        { "04 Radio", "Distant Station",   "RADIO", { { "age", 0.9f }, { "medium", 3 }, { "noise", 0.9f }, { "mix", 0.85f } } },
    };
    return bank;
}

} // namespace nob
