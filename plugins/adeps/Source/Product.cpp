#include "Product.h"

// ADEPS: product info, factory bank and its own interface strings.
// Values: fatness / color / mix 0..1, gain and output in dB, auto_gain 0/1.

namespace
{
const char* translations (const juce::String& code)
{
    if (code.startsWith ("es")) return R"X(
"Compression, drive and clipping, all from one knob" = "Compresión, saturación y clipping, todo en una perilla"
"The character of the drive: dark ladder, creamy OTA or bright Sallen-Key" = "El carácter de la saturación: ladder oscuro, OTA cremoso o Sallen-Key brillante"
"Set it so the lamp glows green, then play with FATNESS and COLOR" = "Ajustala hasta que la lámpara quede en verde, y después jugá con FATNESS y COLOR"
"Keeps the output as loud as the input, so you judge the tone and not the volume" = "Mantiene la salida al mismo volumen que la entrada, así juzgás el sonido y no el volumen"
"TURN GAIN UNTIL THE LAMP IS GREEN" = "GIRÁ GAIN HASTA QUE LA LÁMPARA ESTÉ EN VERDE"
"Instant fatness: compression, drive and clipping in one knob, with the color of three analog filters." = "Gordura instantánea: compresión, saturación y clipping en una perilla, con el color de tres filtros analógicos."
)X";
    if (code == "fr") return R"X(
"Compression, drive and clipping, all from one knob" = "Compression, saturation et écrêtage, d'un seul bouton"
"The character of the drive: dark ladder, creamy OTA or bright Sallen-Key" = "Le caractère de la saturation : ladder sombre, OTA crémeux ou Sallen-Key brillant"
"Set it so the lamp glows green, then play with FATNESS and COLOR" = "Réglez jusqu'à ce que la lampe soit verte, puis jouez avec FATNESS et COLOR"
"Keeps the output as loud as the input, so you judge the tone and not the volume" = "Garde la sortie au niveau de l'entrée : vous jugez le son, pas le volume"
"TURN GAIN UNTIL THE LAMP IS GREEN" = "TOURNEZ GAIN JUSQU'AU VERT"
"Instant fatness: compression, drive and clipping in one knob, with the color of three analog filters." = "Épaisseur instantanée : compression, saturation et écrêtage en un bouton, avec la couleur de trois filtres analogiques."
)X";
    if (code == "zh") return R"X(
"Compression, drive and clipping, all from one knob" = "压缩、驱动和削波，一个旋钮搞定"
"The character of the drive: dark ladder, creamy OTA or bright Sallen-Key" = "驱动的性格：暗沉的 Ladder、柔和的 OTA 或明亮的 Sallen-Key"
"Set it so the lamp glows green, then play with FATNESS and COLOR" = "把灯调到绿色，然后调 FATNESS 和 COLOR"
"Keeps the output as loud as the input, so you judge the tone and not the volume" = "让输出与输入一样响，只比较音色而不是音量"
"TURN GAIN UNTIL THE LAMP IS GREEN" = "调节 GAIN 直到灯变绿"
"Instant fatness: compression, drive and clipping in one knob, with the color of three analog filters." = "瞬间加厚：压缩、驱动和削波集于一个旋钮，带有三种模拟滤波器的色彩。"
)X";
    if (code == "ja") return R"X(
"Compression, drive and clipping, all from one knob" = "コンプ、ドライブ、クリップをひとつのノブで"
"The character of the drive: dark ladder, creamy OTA or bright Sallen-Key" = "ドライブの性格：暗いラダー、クリーミーな OTA、明るい Sallen-Key"
"Set it so the lamp glows green, then play with FATNESS and COLOR" = "ランプが緑になるように合わせ、FATNESS と COLOR で遊ぶ"
"Keeps the output as loud as the input, so you judge the tone and not the volume" = "出力を入力と同じ音量に保ち、音量ではなく音色で判断できます"
"TURN GAIN UNTIL THE LAMP IS GREEN" = "ランプが緑になるまで GAIN を回す"
"Instant fatness: compression, drive and clipping in one knob, with the color of three analog filters." = "一瞬で太く：コンプ、ドライブ、クリップをひとつのノブに。3 種のアナログフィルターの色付き。"
)X";
    return nullptr;
}
}

namespace nob
{

const ProductInfo& product()
{
    static const ProductInfo info { "ADEPS", "INSTANT FATNESS  /  LITE",
                                    "Instant fatness: compression, drive and clipping in one knob, with the color of three analog filters.",
                                    ".adeps", translations };
    return info;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> bank =
    {
        { "", "Init", "", {} },

        { "01 Drums", "Drum Bus Grease",    "DRUMS", { { "fatness", 0.55f }, { "color", 0.65f } } },
        { "01 Drums", "Kick Thump",         "DRUMS", { { "fatness", 0.45f }, { "color", 0.10f }, { "gain", 2.0f } } },
        { "01 Drums", "Snare Crack",        "DRUMS", { { "fatness", 0.6f }, { "color", 0.9f } } },
        { "01 Drums", "Parallel Smash",     "DRUMS", { { "fatness", 0.9f }, { "color", 0.55f }, { "mix", 0.4f }, { "auto_gain", 1 } } },

        { "02 Bass", "808 Weight",          "BASS", { { "fatness", 0.5f }, { "color", 0.0f }, { "auto_gain", 1 } } },
        { "02 Bass", "Reese Growl",         "BASS", { { "fatness", 0.75f }, { "color", 0.7f }, { "auto_gain", 1 } } },
        { "02 Bass", "Round & Warm",        "BASS", { { "fatness", 0.3f }, { "color", 0.2f } } },

        { "03 Synths & Vocals", "Chord Stack Glue", "SYNTH", { { "fatness", 0.4f }, { "color", 0.5f }, { "auto_gain", 1 } } },
        { "03 Synths & Vocals", "Lead Bite",        "SYNTH", { { "fatness", 0.65f }, { "color", 0.95f }, { "auto_gain", 1 } } },
        { "03 Synths & Vocals", "Vocal Density",    "VOCAL", { { "fatness", 0.25f }, { "color", 0.45f }, { "mix", 0.6f }, { "auto_gain", 1 } } },

        { "04 Mix & Master", "Master Butter (8%)",  "MASTER", { { "fatness", 0.08f }, { "color", 0.4f } } },
        { "04 Mix & Master", "Bus Warmth",          "MASTER", { { "fatness", 0.18f }, { "color", 0.15f }, { "auto_gain", 1 } } },
        { "04 Mix & Master", "Maximum Fat",        "LOUD",   { { "fatness", 1.0f }, { "color", 0.6f }, { "output", -3.0f } } },
    };
    return bank;
}

} // namespace nob
