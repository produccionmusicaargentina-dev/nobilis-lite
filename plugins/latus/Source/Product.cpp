#include "Product.h"

// LATUS: product info, factory bank and its own interface strings.
// Values: width / age / mix 0..1, mode 0 I 1 II 2 I+II 3 ENSEMBLE, speed x, bass_mono 0/1, dB.

namespace
{
const char* translations (const juce::String& code)
{
    if (code.startsWith ("es")) return R"X(
"How much chorus: wet level and depth together" = "Cuánto chorus: nivel y profundidad juntos"
"Scales the speed of the chosen mode" = "Escala la velocidad del modo elegido"
"The bucket-brigade chip: darker, more saturated and a little hiss" = "El chip bucket-brigade: más oscuro, más saturado y un poco de soplido"
"Keeps everything under 150 Hz dry and centred" = "Deja todo lo que está debajo de 150 Hz seco y al centro"
"The classic poly-synth bucket-brigade chorus in one knob, from a slow wide wash to a string ensemble." = "El clásico chorus bucket-brigade de los sintes polifónicos en una perilla, de un barrido lento y ancho a un ensemble de cuerdas."
)X";
    if (code == "fr") return R"X(
"How much chorus: wet level and depth together" = "Quantité de chorus : niveau et profondeur ensemble"
"Scales the speed of the chosen mode" = "Ajuste la vitesse du mode choisi"
"The bucket-brigade chip: darker, more saturated and a little hiss" = "La puce bucket-brigade : plus sombre, plus saturée, un peu de souffle"
"Keeps everything under 150 Hz dry and centred" = "Garde tout sous 150 Hz sec et au centre"
"The classic poly-synth bucket-brigade chorus in one knob, from a slow wide wash to a string ensemble." = "Le chorus bucket-brigade des synthés polyphoniques en un bouton, du balayage lent et large à l'ensemble de cordes."
)X";
    if (code == "zh") return R"X(
"How much chorus: wet level and depth together" = "合唱的量：湿声电平与深度一起调"
"Scales the speed of the chosen mode" = "调整所选模式的速度"
"The bucket-brigade chip: darker, more saturated and a little hiss" = "BBD 芯片：更暗、更饱和，带一点底噪"
"Keeps everything under 150 Hz dry and centred" = "150 Hz 以下保持干声并居中"
"The classic poly-synth bucket-brigade chorus in one knob, from a slow wide wash to a string ensemble." = "经典复音合成器 BBD 合唱，一个旋钮，从缓慢宽阔到弦乐合奏。"
)X";
    if (code == "ja") return R"X(
"How much chorus: wet level and depth together" = "コーラスの量：ウェットレベルと深さを同時に"
"Scales the speed of the chosen mode" = "選んだモードの速さを調整"
"The bucket-brigade chip: darker, more saturated and a little hiss" = "BBD チップ：より暗く、より飽和し、少しヒス"
"Keeps everything under 150 Hz dry and centred" = "150 Hz 以下をドライのまま中央に"
"The classic poly-synth bucket-brigade chorus in one knob, from a slow wide wash to a string ensemble." = "ポリシンセの定番 BBD コーラスをワンノブで。ゆったり広がる音からストリングスアンサンブルまで。"
)X";
    return nullptr;
}
}

namespace nob
{

const ProductInfo& product()
{
    static const ProductInfo info { "LATUS", "ENSEMBLE CHORUS  /  LITE",
                                    "The classic poly-synth bucket-brigade chorus in one knob, from a slow wide wash to a string ensemble.",
                                    ".latus", translations };
    return info;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> bank =
    {
        { "", "Init", "", {} },

        { "01 Synths", "Poly Pad I",          "CLASSIC", { { "width", 0.75f }, { "mode", 0 } } },
        { "01 Synths", "Poly Pad II",         "CLASSIC", { { "width", 0.75f }, { "mode", 1 } } },
        { "01 Synths", "Shimmer I + II",      "CLASSIC", { { "width", 0.6f }, { "mode", 2 } } },
        { "01 Synths", "String Machine",      "ENSEMBLE", { { "width", 0.85f }, { "mode", 3 }, { "age", 0.4f } } },

        { "02 Keys & Guitars", "Rhodes Swirl",   "CLASSIC", { { "width", 0.55f }, { "mode", 0 }, { "speed", 1.4f } } },
        { "02 Keys & Guitars", "Clean Guitar 80s", "CLASSIC", { { "width", 0.65f }, { "mode", 1 }, { "age", 0.1f } } },
        { "02 Keys & Guitars", "Vibrato Keys",   "FAST",    { { "width", 0.8f }, { "mode", 2 }, { "speed", 0.7f }, { "mix", 1.0f } } },

        { "03 Bass & Vocals", "Bass Width (Safe)", "MONO-SAFE", { { "width", 0.5f }, { "mode", 0 }, { "bass_mono", 1 } } },
        { "03 Bass & Vocals", "Vocal Double",      "SUBTLE",    { { "width", 0.35f }, { "mode", 3 }, { "age", 0.05f }, { "mix", 0.6f } } },

        { "04 Lo-Fi & Texture", "Old Chip",      "LOFI", { { "width", 0.7f }, { "mode", 0 }, { "age", 0.9f } } },
        { "04 Lo-Fi & Texture", "Seasick Tape",  "LOFI", { { "width", 1.0f }, { "mode", 1 }, { "speed", 0.35f }, { "age", 0.6f } } },
        { "04 Lo-Fi & Texture", "Wide Mix Bus",  "SUBTLE", { { "width", 0.2f }, { "mode", 0 }, { "age", 0.0f } } },
    };
    return bank;
}

} // namespace nob
