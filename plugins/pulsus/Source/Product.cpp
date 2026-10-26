#include "Product.h"

// PULSUS: product info, factory bank and its own interface strings.
// Values are real (denormalised): choices by index (rate 0 1/32 1 1/16 2 1/8T 3 1/8
// 4 1/4T 5 1/4 6 1/2 7 1 BAR 8 2 BARS 9 4 BARS; shape 0 PUMP 1 SOFT 2 RAMP 3 CHOP;
// band 0 FULL 1 LOWS 2 HIGHS), 0..1 for depth / shift / breathe / mix, Hz, dB.

namespace
{
const char* translations (const juce::String& code)
{
    if (code.startsWith ("es")) return R"X(
"How deep the volume dips on every step" = "Cuánto baja el volumen en cada paso"
"Moves the dip off the beat, for off-beat and counter-rhythm pumps" = "Corre el pozo fuera del tiempo, para bombeos a contratiempo"
"A low-pass that closes with the dip and opens as the sound comes back" = "Un pasa-bajos que se cierra con el pozo y se abre cuando vuelve el sonido"
"Split frequency for the LOWS / HIGHS bands" = "Frecuencia de corte de las bandas LOWS / HIGHS"
"Tempo-synced pumping without a sidechain: pick a rate and a shape, then turn DEPTH." = "Bombeo sincronizado al tempo sin sidechain: elegí la rítmica y la forma, y girá DEPTH."
)X";
    if (code == "fr") return R"X(
"How deep the volume dips on every step" = "Profondeur de la baisse de volume à chaque pas"
"Moves the dip off the beat, for off-beat and counter-rhythm pumps" = "Décale le creux hors du temps, pour des pompages à contretemps"
"A low-pass that closes with the dip and opens as the sound comes back" = "Un passe-bas qui se ferme avec le creux et s'ouvre quand le son revient"
"Split frequency for the LOWS / HIGHS bands" = "Fréquence de séparation des bandes LOWS / HIGHS"
"Tempo-synced pumping without a sidechain: pick a rate and a shape, then turn DEPTH." = "Pompage synchronisé au tempo sans sidechain : choisissez le rythme et la forme, puis tournez DEPTH."
)X";
    if (code == "zh") return R"X(
"How deep the volume dips on every step" = "每一步音量下压的深度"
"Moves the dip off the beat, for off-beat and counter-rhythm pumps" = "把下压移到拍点之外，做反拍律动"
"A low-pass that closes with the dip and opens as the sound comes back" = "低通滤波随下压关闭，声音回来时打开"
"Split frequency for the LOWS / HIGHS bands" = "LOWS / HIGHS 分频点"
"Tempo-synced pumping without a sidechain: pick a rate and a shape, then turn DEPTH." = "无需侧链的速度同步泵感：选择节奏和形状，然后转动 DEPTH。"
)X";
    if (code == "ja") return R"X(
"How deep the volume dips on every step" = "ステップごとの音量の沈み込みの深さ"
"Moves the dip off the beat, for off-beat and counter-rhythm pumps" = "ディップを拍からずらして裏拍のポンピングに"
"A low-pass that closes with the dip and opens as the sound comes back" = "ディップで閉じ、音が戻ると開くローパス"
"Split frequency for the LOWS / HIGHS bands" = "LOWS / HIGHS バンドの分割周波数"
"Tempo-synced pumping without a sidechain: pick a rate and a shape, then turn DEPTH." = "サイドチェイン不要のテンポ同期ポンピング。レートとシェイプを選び、DEPTH を回すだけ。"
)X";
    return nullptr;
}
}

namespace nob
{

const ProductInfo& product()
{
    static const ProductInfo info { "PULSUS", "TEMPO PUMP  /  LITE",
                                    "Tempo-synced pumping without a sidechain: pick a rate and a shape, then turn DEPTH.",
                                    ".pulsus", translations };
    return info;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> bank =
    {
        { "", "Init", "", {} },

        { "01 House & Techno", "Four On The Floor", "PUMP", { { "depth", 0.75f }, { "rate", 5 } } },
        { "01 House & Techno", "Offbeat Bounce",    "PUMP", { { "depth", 0.6f }, { "rate", 5 }, { "shape", 1 }, { "shift", 0.5f } } },
        { "01 House & Techno", "Breathing Pad",     "PUMP,FILTER", { { "depth", 0.6f }, { "rate", 5 }, { "breathe", 0.7f } } },
        { "01 House & Techno", "Big Room Squash",   "PUMP", { { "depth", 0.95f }, { "rate", 5 }, { "output", 1.5f } } },

        { "02 Bass", "Kick Room (Lows Only)", "PUMP,BAND", { { "depth", 0.9f }, { "rate", 5 }, { "band", 1 }, { "xover", 180.0f } } },
        { "02 Bass", "Wub 1/8",               "FILTER",    { { "depth", 0.85f }, { "rate", 3 }, { "shape", 1 }, { "breathe", 0.85f } } },
        { "02 Bass", "Triplet Wobble",        "FILTER",    { { "depth", 0.7f }, { "rate", 4 }, { "shape", 1 }, { "breathe", 0.6f } } },

        { "03 Trance & Gates", "Trance Gate 1/16", "GATE", { { "depth", 1.0f }, { "rate", 1 }, { "shape", 3 } } },
        { "03 Trance & Gates", "Stutter 1/32",     "GATE", { { "depth", 0.9f }, { "rate", 0 }, { "shape", 3 }, { "mix", 0.8f } } },
        { "03 Trance & Gates", "Half-Time Chop",   "GATE", { { "depth", 0.8f }, { "rate", 6 }, { "shape", 3 } } },

        { "04 Subtle & Mix Bus", "Mix Bus Breath",   "SUBTLE", { { "depth", 0.18f }, { "rate", 5 } } },
        { "04 Subtle & Mix Bus", "Slow Swell (Bar)", "SUBTLE", { { "depth", 0.7f }, { "rate", 7 }, { "shape", 2 } } },
        { "04 Subtle & Mix Bus", "Hats Make Room",   "SUBTLE,BAND", { { "depth", 0.45f }, { "rate", 3 }, { "band", 2 }, { "xover", 600.0f } } },
        { "04 Subtle & Mix Bus", "Lo-Fi Sway",       "SUBTLE,FILTER", { { "depth", 0.35f }, { "rate", 6 }, { "shape", 1 }, { "breathe", 0.5f } } },
    };
    return bank;
}

} // namespace nob
