#include "Presets.h"

namespace vrac
{
    const std::array<FactoryPreset, 6>& factoryPresets()
    {
        static const std::array<FactoryPreset, 6> presets = []
        {
            std::array<FactoryPreset, 6> p {};

            p[0].name = "BIG CAT";
            p[0].tag = "FOLEY BREAK";
            p[0].sourceId = "foley";
            p[0].sensitivity = 0.55f;
            p[0].selectedPad = 3;
            p[0].global = { 0.0f, 104.0f, -2.4f, 0.0f, 1.0f, 0.0f, 0.0f, 0.68f };
            p[0].pads[0] = { .modelIndex = 0, .drumify = 0.70f, .length = 0.46f };
            p[0].pads[1] = { .modelIndex = 1, .drumify = 0.62f, .length = 0.27f, .pitch = -1.0f };
            p[0].pads[2] = { .modelIndex = 5, .drumify = 0.76f, .length = 0.20f };
            p[0].pads[3] = { .modelIndex = 12, .drumify = 0.78f, .length = 0.31f };
            p[0].pads[6] = { .modelIndex = 8, .drumify = 0.86f, .length = 0.085f, .pan = 0.18f, .choke = 2 };
            p[0].pads[8] = { .modelIndex = 9, .drumify = 0.86f, .length = 0.33f, .pan = 0.30f, .choke = 2 };
            p[0].pads[15] = { .modelIndex = 12, .drumify = 0.82f, .length = 0.27f };

            p[1].name = "NIGHT KITCHEN";
            p[1].tag = "CLATTER";
            p[1].sourceId = "kitchen";
            p[1].sensitivity = 0.64f;
            p[1].selectedPad = 3;
            p[1].global = { 0.0f, 116.0f, -3.2f, 0.0f, 0.88f, -0.8f, -0.11f, 0.78f };
            p[1].pads[0] = { .modelIndex = 1, .drumify = 0.80f, .length = 0.30f };
            p[1].pads[1] = { .modelIndex = 0, .drumify = 0.64f, .length = 0.40f, .pitch = 2.0f };
            p[1].pads[2] = { .modelIndex = 5, .drumify = 0.82f, .length = 0.17f };
            p[1].pads[3] = { .modelIndex = 13, .drumify = 0.86f, .length = 0.19f };
            p[1].pads[6] = { .modelIndex = 8, .drumify = 0.90f, .length = 0.055f, .pan = 0.24f, .choke = 2 };
            p[1].pads[8] = { .modelIndex = 9, .drumify = 0.90f, .length = 0.22f, .pan = 0.32f, .choke = 2 };
            p[1].pads[10] = { .modelIndex = 15, .drumify = 0.84f, .length = 0.13f };
            p[1].pads[15] = { .modelIndex = 14, .drumify = 0.78f, .length = 0.36f };

            p[2].name = "BODY TALK";
            p[2].tag = "HUMAN PERC";
            p[2].sourceId = "body";
            p[2].sensitivity = 0.50f;
            p[2].selectedPad = 2;
            p[2].global = { 0.0f, 96.0f, -2.8f, 0.0f, 1.12f, -2.4f, 0.0f, 0.62f };
            p[2].pads[0] = { .modelIndex = 2, .drumify = 0.60f, .length = 0.52f, .pitch = -1.0f };
            p[2].pads[1] = { .modelIndex = 0, .drumify = 0.52f, .length = 0.44f, .pitch = -2.0f };
            p[2].pads[2] = { .modelIndex = 6, .drumify = 0.62f, .length = 0.34f, .pan = 0.08f };
            p[2].pads[3] = { .modelIndex = 14, .drumify = 0.66f, .length = 0.44f };
            p[2].pads[6] = { .modelIndex = 11, .drumify = 0.76f, .length = 0.09f, .pan = 0.20f, .choke = 2 };
            p[2].pads[8] = { .modelIndex = 9, .drumify = 0.62f, .length = 0.40f, .choke = 2 };
            p[2].pads[10] = { .modelIndex = 12, .drumify = 0.70f, .length = 0.32f };
            p[2].pads[11] = { .modelIndex = 14, .drumify = 0.56f, .length = 0.50f };
            p[2].pads[15] = { .modelIndex = 14, .drumify = 0.68f, .length = 0.40f };

            p[3].name = "IRON WORKS";
            p[3].tag = "HEAVY METAL";
            p[3].sourceId = "scrap";
            p[3].sensitivity = 0.70f;
            p[3].selectedPad = 0;
            p[3].global = { 0.0f, 138.0f, -4.1f, 0.0f, 0.80f, 2.2f, 0.04f, 0.90f };
            p[3].pads[0] = { .modelIndex = 3, .drumify = 0.92f, .length = 0.35f };
            p[3].pads[1] = { .modelIndex = 1, .drumify = 0.88f, .length = 0.24f, .pitch = -3.0f };
            p[3].pads[2] = { .modelIndex = 4, .drumify = 0.90f, .length = 0.26f };
            p[3].pads[3] = { .modelIndex = 15, .drumify = 0.92f, .length = 0.20f };
            p[3].pads[6] = { .modelIndex = 10, .drumify = 0.95f, .length = 0.07f, .pan = 0.26f, .choke = 2 };
            p[3].pads[8] = { .modelIndex = 9, .drumify = 0.92f, .length = 0.24f, .pan = 0.34f, .choke = 2 };
            p[3].pads[10] = { .modelIndex = 15, .drumify = 0.90f, .length = 0.15f };
            p[3].pads[15] = { .modelIndex = 14, .drumify = 0.90f, .length = 0.32f };

            p[4].name = "SHORT CIRCUIT";
            p[4].tag = "GLITCH";
            p[4].sourceId = "circuit";
            p[4].sensitivity = 0.76f;
            p[4].selectedPad = 10;
            p[4].global = { 0.0f, 122.0f, -3.6f, -3.0f, 0.64f, 2.8f, 0.07f, 0.86f };
            p[4].pads[0] = { .modelIndex = 1, .drumify = 0.80f, .length = 0.20f };
            p[4].pads[1] = { .modelIndex = 3, .drumify = 0.90f, .length = 0.16f, .pitch = 5.0f };
            p[4].pads[2] = { .modelIndex = 5, .drumify = 0.88f, .length = 0.12f };
            p[4].pads[3] = { .modelIndex = 15, .drumify = 0.90f, .length = 0.10f, .pitch = 4.0f };
            p[4].pads[6] = { .modelIndex = 8, .drumify = 0.95f, .length = 0.045f, .pan = 0.20f, .choke = 2 };
            p[4].pads[8] = { .modelIndex = 9, .drumify = 0.94f, .length = 0.15f, .choke = 2 };
            p[4].pads[10] = { .modelIndex = 13, .drumify = 0.94f, .length = 0.09f, .pitch = 3.0f };
            p[4].pads[11] = { .modelIndex = 15, .drumify = 0.90f, .length = 0.13f };
            p[4].pads[14] = { .modelIndex = 7, .drumify = 0.92f, .length = 0.09f, .pitch = 9.0f };
            p[4].pads[15] = { .modelIndex = 12, .drumify = 0.90f, .length = 0.18f };

            p[5].name = "DUST LOOP";
            p[5].tag = "LO-FI";
            p[5].sourceId = "vinyl";
            p[5].sensitivity = 0.45f;
            p[5].selectedPad = 15;
            p[5].global = { -12.0f, 44.0f, -2.6f, 0.0f, 1.35f, -4.2f, -0.17f, 0.52f };
            p[5].pads[0] = { .modelIndex = 2, .drumify = 0.34f, .length = 0.55f, .pitch = -3.0f, .drumifiedGain = -8.5f, .originalGain = -1.2f };
            p[5].pads[1] = { .modelIndex = 0, .drumify = 0.28f, .length = 0.48f, .pitch = -2.0f, .drumifiedGain = -9.0f, .originalGain = -1.6f };
            p[5].pads[2] = { .modelIndex = 6, .drumify = 0.38f, .length = 0.36f, .drumifiedGain = -8.6f, .originalGain = -1.4f };
            p[5].pads[3] = { .modelIndex = 12, .drumify = 0.34f, .length = 0.44f, .drumifiedGain = -9.0f, .originalGain = -2.0f };
            p[5].pads[6] = { .modelIndex = 8, .drumify = 0.42f, .length = 0.11f, .pan = 0.18f, .choke = 2, .drumifiedGain = -8.0f, .originalGain = -2.0f };
            p[5].pads[8] = { .modelIndex = 9, .drumify = 0.36f, .length = 0.40f, .choke = 2, .drumifiedGain = -8.5f, .originalGain = -2.2f };
            p[5].pads[10] = { .modelIndex = 15, .drumify = 0.40f, .length = 0.22f, .drumifiedGain = -8.2f, .originalGain = -1.8f };
            p[5].pads[15] = { .modelIndex = 14, .drumify = 0.30f, .length = 0.52f, .drumifiedGain = -9.0f, .originalGain = -1.6f };

            return p;
        }();
        return presets;
    }
} // namespace vrac
