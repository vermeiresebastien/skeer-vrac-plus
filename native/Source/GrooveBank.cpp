#include "Sequencer.h"

namespace sliceforge
{
    /*  The factory groove bank: twenty genres, three named variations each, every one written across
        the whole 32-step grid as two bars that differ. Each lane mask is two 16-character halves -
        bar one then bar two - so the second bar is visible as a development when a groove is read,
        and the bank check in Tests/GrooveBankTest.cpp holds every mask to exactly kSeqSteps
        characters from the documented alphabet.

        Division is 4 (a step is a sixteenth) for every groove on purpose: 32 steps is exactly two
        bars only at that division, and a genre whose two bars are not four beats each cannot keep
        the "spans the full grid" promise. Triplet and shuffled feels come from `swing`, which the
        scheduler applies to odd steps, and from the '<' and '>' push characters - not from a
        division that would move the bar line. */
    namespace
    {
        // The sixteen models, spelled the way `pad.model` numbers them: family * 4 + local.
        constexpr int kKickRound = 0, kKickPunch = 1, kKick808 = 2, kKickBroken = 3;
        constexpr int kSnareCrack = 4, kSnareTight = 5, kSnareBrush = 6, kSnareRim = 7;
        constexpr int kHatClosed = 8, kHatOpen = 9, kHatMetal = 10, kHatAir = 11;
        constexpr int kClapClassic = 12, kClapTight = 13, kClapBig = 14, kClapSnap = 15;

        // ---------------------------------------------------------------- HOUSE

        constexpr GrooveLane houseClassic[] = {
            { 0, "X...x...X...x..." "X...x...X...x.X." },
            { 8, "..x...x...x...x." "..x...x...x...x." },
            { 6, ".-.-.-.-.-.-.-.-" ".-.-.-.-.-.-.-.-" },
            { 3, "....X.......X..." "....X.......X..x" },
            { 10, "................" "......x.....x..." },
        };
        constexpr GrooveLane houseDeep[] = {
            { 0, "X.......X...x..." "X.......X...x..-" },
            { 8, "..x...x...x...x." "..x...x...x...x." },
            { 7, "..-...p...-...p." "..-...p...-...p." },
            { 3, "....x.......x..." "............x..." },
            { 14, "................" "......x........x" },
        };
        constexpr GrooveLane houseDrive[] = {
            { 0, "X...x...X...x..." "X...x...X...x.x." },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-x-x-xX" },
            { 8, "..X...X...X...X." "..X...X...X...X." },
            { 3, "....X.......X..." "....X.......X..." },
            { 10, "..-...-...-...-." "..-...-...-...-." },
            { 11, "................" "....x..x..x....." },
        };
        constexpr GrooveVoice houseVoicing[] = {
            { 0, kKickPunch }, { 8, kHatOpen }, { 3, kClapClassic }, { 6, kHatClosed },
        };

        // ---------------------------------------------------------------- TECHNO

        constexpr GrooveLane technoDrive[] = {
            { 0, "X...X...X...X..." "X...X...X...X..." },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-x-x-xX" },
            { 9, "..X...X...X...X." "..X...X...X...X." },
            { 3, "....x.......x..." "....x.......x..x" },
            { 12, "-...-...-...-..." "....-...-...-..." },
        };
        constexpr GrooveLane technoMachine[] = {
            { 0, "X..xX...X..xX..." "X..xX...X..xX..x" },
            { 9, "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.x.x." },
            { 6, ".-.-.-.-.-.-.-.-" ".-.-.-.-.-.-.-.x" },
            { 14, "....-...-...-..." "..............-." },
            { 15, "................" "............x..." },
        };
        constexpr GrooveLane technoDub[] = {
            { 0, "X...X...X...X..." "X...X...X...X..." },
            { 8, "..x.......x....." "..x.......x....." },
            { 12, "....-.......-..." "......-.......-." },
            { 3, "................" "......p....p...." },
            { 10, "................" "...p........p..." },
        };
        constexpr GrooveVoice technoVoicing[] = {
            { 0, kKickPunch }, { 9, kHatMetal }, { 6, kHatClosed }, { 3, kClapTight },
        };

        // ---------------------------------------------------------------- TRAP

        constexpr GrooveLane trapDark[] = {
            { 0, "X..x..x...x..x.." "X..x..x...x.x..x" },
            { 3, "........X......." "........X.....x." },
            { 6, "xXxxXxXrxXxxXxXr" "xXxxXxXrxXxxXxrx" },
            { 1, "................" "...x........x..." },
            { 14, "..............-." "..........-....." },
        };
        constexpr GrooveLane trapRoller[] = {
            { 0, "X...x.....x..x.." "X...x.....x.x..." },
            { 2, "........X......." "........X.....x." },
            { 6, "xXrrXxXrxxXrrXxr" "xXrrXxXrxxXrRxXx" },
            { 10, "..-.......-....." "..-.......-....." },
            { 1, ".......x........" "....x.......x..." },
        };
        constexpr GrooveLane trapDrill[] = {
            { 0, "X.....x...x....." "X.....x...x...x." },
            { 3, "........X......." "........X...x..." },
            { 6, "x.xxXxx.x.xxXxx." "x.xxXxx.x.xRxrx." },
            { 14, "......-.....-..." "................" },
            { 1, "...x........x..." "...x......x....." },
        };
        constexpr GrooveVoice trapVoicing[] = {
            { 0, kKick808 }, { 1, kKickRound }, { 3, kClapTight }, { 6, kHatClosed },
        };

        // ---------------------------------------------------------------- BOOM BAP

        constexpr GrooveLane boomBapDusty[] = {
            { 0, "X.......x...x..." "X.......x...x..-" },
            { 2, "....X.......X..." "....X.......X..." },
            { 6, "x.x.x.-.x.x.x.-." "x.x.x.-.x.x.x.xx" },
            { 14, "......-........." ".........-......" },
            { 10, "................" "...........-...-" },
        };
        constexpr GrooveLane boomBapJazzy[] = {
            { 0, "X.......x....x.." "X.......x....x.." },
            { 5, "....x...x...x..." "....x.p.x...x..p" },
            { 13, "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.x.x." },
            { 14, "......x.....x..." ".........x...x.." },
            { 12, "................" "....-.....-....." },
        };
        constexpr GrooveLane boomBapDilla[] = {
            { 0, "X..>..x...>.x..>" "X.>..x..>.x..>.." },
            { 2, "....X<......X<.." "....X<......X<.." },
            { 6, "x.-.x.-.x.-.x.-." "x.-.x.-.x.-.x.xx" },
            { 4, "..............-<" "......-<........" },
            { 10, "................" "...p.......p...." },
        };
        constexpr GrooveVoice boomBapVoicing[] = {
            { 0, kKickRound }, { 2, kSnareCrack }, { 6, kHatClosed }, { 5, kSnareBrush },
        };

        // ---------------------------------------------------------------- DRUM & BASS

        constexpr GrooveLane dnbAmen[] = {
            { 0, "X.....x...x....." "X.....x...x..x.." },
            { 2, "....X..x..X...x." "....X..x..X..x.x" },
            { 6, "-.-x-.-.-.-x-.-." "-.-x-.-.-.-x-.-x" },
            { 14, "..........-....." ".......-........" },
            { 8, "....-.......-..." "................" },
        };
        constexpr GrooveLane dnbRoller[] = {
            { 0, "X.-.x.-.X.-.x.-." "X.-.x.-.X.-.x.-x" },
            { 2, "....X.......X..." "....X.......X..x" },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-x-x-x-" },
            { 4, "..........x....." "......x.....x..." },
            { 12, "................" "....-.....-...-." },
        };
        constexpr GrooveLane dnbHalftime[] = {
            { 0, "X.......x......." "X.......x......-" },
            { 2, "........X......." "........X......." },
            { 6, "..-...-...-...-." "..-...-...-...-x" },
            { 12, "....p.......p..." "................" },
            { 8, "..............x." "......x........." },
        };
        constexpr GrooveVoice dnbVoicing[] = {
            { 0, kKickPunch }, { 2, kSnareCrack }, { 4, kSnareTight }, { 6, kHatClosed },
        };

        // ---------------------------------------------------------------- UK GARAGE

        constexpr GrooveLane garageTwoStep[] = {
            { 0, "X.....x...x....." "X.....x...x..x.." },
            { 3, "....X.......X..." "....X.....x.X..." },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-xXx-x-" },
            { 8, "..x.......x....." "..x.......x....x" },
            { 10, "..........-....." "..-...-.....-..." },
        };
        constexpr GrooveLane garageShuffle[] = {
            { 0, "X.......x..x...." "X.......x..x..x." },
            { 2, "....x.......x..." "....x...x...x.x." },
            { 7, "-x-x-x-x-x-x-x-x" "-x-x-x-x-xXx-x-x" },
            { 8, "..x...x...x...x." "..x...x...x...x." },
            { 12, "................" "......p.......p." },
        };
        constexpr GrooveLane garageNighty[] = {
            { 0, "X.....x.....x..." "X.....x.....x..-" },
            { 3, "....X.......X.X." "....X.......X.X." },
            { 6, "x.-.x.-.x.-.x.-." "x.-.x.-.x.-.xx.x" },
            { 11, "................" "............x..." },
            { 14, "...-........-..." "...-.......-..-." },
            { 12, "....p..........." "......p.....p..." },
        };
        constexpr GrooveVoice garageVoicing[] = {
            { 0, kKickPunch }, { 3, kClapClassic }, { 6, kHatClosed }, { 8, kHatOpen },
        };

        // ---------------------------------------------------------------- AFRO

        constexpr GrooveLane afroTwelve[] = {
            { 0, "X.....x...x....." "X.....x...x..x.." },
            { 2, "....X.......X..." "....X.......X..x" },
            { 6, "x.xx-x--x-x-", 12 },
            { 7, "..x...x...x.", 12 },
            { 10, "x..x..", 6 },
            { 12, "................" "......p....p...." },
        };
        constexpr GrooveLane afroHouse[] = {
            { 0, "X.....x...x....." "X.....x...x...x." },
            { 8, "..x...x...x...x." "..x...x...x...x." },
            { 6, "x.-.x.-.x.-.", 12 },
            { 3, "....x.......x..." "....x.....x.x..." },
            { 12, "....-.......-..." "........-.....-." },
            { 10, "................" "..x.......x....." },
        };
        constexpr GrooveLane amapiano[] = {
            { 0, "X.......x...x..." "X.......x...x..-" },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-x-xXx-" },
            { 10, "..x..x...x..x..." "..x..x...x..x..x" },
            { 14, "................" "......x.....x..." },
            { 3, "....x..........." "....x........x.." },
            { 12, "................" "...p........p..." },
        };
        constexpr GrooveVoice afroVoicing[] = {
            { 0, kKickRound }, { 6, kHatClosed }, { 10, kClapSnap }, { 12, kHatAir },
        };

        // ---------------------------------------------------------------- DANCEHALL

        constexpr GrooveLane dancehallDembow[] = {
            { 0, "X.....x...X....." "X.....x...X..x.." },
            { 3, "....X.......X..." "....X.......X..x" },
            { 6, "-x-x-x-x-x-x-x-x" "-x-x-x-x-x-x-xXx" },
            { 10, "..x....x..x....." "......x....x...." },
            { 14, "......x.....x..." "................" },
        };
        constexpr GrooveLane dancehallPony[] = {
            { 0, "X...x..xX...x..x" "X...x..xX...x.x." },
            { 2, "....X.......X..." "....X.......X..." },
            { 9, "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.xXx." },
            { 11, "................" "......x.....x.x." },
            { 4, "................" "...-........-..." },
        };
        constexpr GrooveLane dancehallFusion[] = {
            { 0, "X.....x...x....." "X..x..x...x..x.." },
            { 3, "....X.......X..." "....X...x...X..." },
            { 9, "..x...x...x...x." "..x...x...x...x." },
            { 6, "..-...-...-...-." "..-...-...-...x." },
            { 12, "................" "......p.......p." },
            { 14, "...-.......-...." "...-....-....-.." },
        };
        constexpr GrooveVoice dancehallVoicing[] = {
            { 0, kKickPunch }, { 3, kClapTight }, { 14, kSnareRim },
        };

        // ---------------------------------------------------------------- BREAKS

        constexpr GrooveLane breaksChoppa[] = {
            { 0, "X..x..x....x..x." "X..x..x....x.x.." },
            { 2, "....X..x..X...x." "....X..x..X..x.x" },
            { 6, "-.-x-.-x-.-x-.-." "-.-x-.-x-.-x-.-x" },
            { 14, "..........-....." ".......-.....-.." },
            { 5, "................" "....x..........." },
        };
        constexpr GrooveLane breaksSkilled[] = {
            { 0, "X....x..x....x.." "X....x..x..x.x.." },
            { 2, "....X.......X..x" "....X...x...X..." },
            { 6, "xXxxXxrxXxxXxXrx" "xXxxXxrxXxRxXxrx" },
            { 4, "..........x....." "......x.....x..." },
            { 12, "-...-...-...-..." "....-...-...-..-" },
        };
        constexpr GrooveLane breaksMinimal[] = {
            { 0, "X.......x......." "X.......x......-" },
            { 2, "....X.......X..." "....X..........." },
            { 6, "..-...-...-...-." "..-...-...-...x." },
            { 14, "................" ".........-......" },
            { 8, "..............x." "......x........." },
        };
        constexpr GrooveVoice breaksVoicing[] = {
            { 0, kKickPunch }, { 2, kSnareCrack }, { 4, kSnareTight }, { 6, kHatClosed },
        };

        // ---------------------------------------------------------------- ELECTRO

        constexpr GrooveLane electroClassic[] = {
            { 0, "X..x....x..x...." "X..x....x..x..x." },
            { 2, "....X...x...X..." "....X...x...X..x" },
            { 9, "..X...X...X...X." "..X...X...X...X." },
            { 10, "................" "......x.....x..." },
            { 15, "................" "............x..." },
        };
        constexpr GrooveLane electroOmni[] = {
            { 0, "X...X..xX...X..." "X...X..xX...X.x." },
            { 2, "....X.......X..." "....X.......X..." },
            { 9, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-x-xRrX" },
            { 6, "-x-x-x-x-x-x-x-x" "-x-x-x-x-x-x-x-x" },
            { 4, "..........x....." "......x........." },
        };
        constexpr GrooveLane electroRecall[] = {
            { 0, "X.....x...x....." "X.....x...x...x." },
            { 3, "....X.......X..." "....X...x...X..." },
            { 11, "....x.......x..." "....x.......x..p" },
            { 6, "x.-.x.-.x.-.x.-." "x.-.x.-.x.-.xx.x" },
            { 12, "................" "..-.......-....." },
        };
        constexpr GrooveVoice electroVoicing[] = {
            { 0, kKick808 }, { 2, kSnareCrack }, { 9, kHatMetal }, { 15, kClapBig },
        };

        // ---------------------------------------------------------------- REGGAE & DUB

        constexpr GrooveLane reggaeOneDrop[] = {
            { 0, "........X......." "........X.....x." },
            { 2, "........x......." "........x...x..." },
            { 6, "..x...x...x...x." "..x...x...x..xx." },
            { 8, "................" "x.......x......." },
            { 12, "................" "...p........p..." },
            { 14, "...>.........<.." "................" },
        };
        constexpr GrooveLane reggaeSteppers[] = {
            { 0, "X...x.....x...x." "X...x...x...x..x" },
            { 6, "..x...x...x...x." "..x...x...x..xx." },
            { 9, "......x........." "......x.....x..." },
            { 12, "................" "p.......p......." },
            { 14, "....x.......x..x" "....x...x...x..." },
        };
        constexpr GrooveLane reggaeDubPlate[] = {
            { 0, "X...........x..." "X.......X......." },
            { 2, "..x...x........." "......x........." },
            { 6, "..-...-...-...-." "..-...-...-...-x" },
            { 8, "p.......p......." "p.......-......." },
            { 10, "................" "....r..........." },
            { 14, "................" "......x.....x..x" },
        };
        constexpr GrooveVoice reggaeVoicing[] = {
            { 0, kKickRound }, { 2, kSnareRim }, { 6, kHatClosed }, { 8, kHatOpen },
            { 9, kHatOpen }, { 12, kHatAir }, { 14, kSnareRim },
        };

        // ---------------------------------------------------------------- FUNK & SOUL

        constexpr GrooveLane funkFunkmaster[] = {
            { 0, "X..x..x...X....x" "X..x..x...x..x.." },
            { 2, "....x.......X..." "....x.......X..r" },
            { 4, "..x.x..x..x.x..x" "..x.x..x..x.x.x." },
            { 6, "x.x.x.x.x.x.x.x." "x.x.x.x.x.xXx.x." },
            { 14, "................" "......x.....x..." },
        };
        constexpr GrooveLane funkSoulBallad[] = {
            { 0, "X..............." "X...........x..." },
            { 2, "........x......." "........x...x..." },
            { 3, "....x.......x..." "....x.......x..f" },
            { 8, "x...x...x...x..." "x...x...x...x..-" },
            { 12, "................" "..............p." },
        };
        constexpr GrooveLane funkSixteenths[] = {
            { 0, "X.x..x..x.x..x.." "X.x..x..x..x.x.." },
            { 2, "....X.......X..." "....X...x...X..x" },
            { 4, "..x.x..x..x.x..x" "..x.x..x..-x.x.x" },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-x-xXx-" },
            { 14, "................" ".......>........" },
        };
        constexpr GrooveVoice funkVoicing[] = {
            { 0, kKickPunch }, { 2, kSnareCrack }, { 4, kSnareTight },
            { 6, kHatClosed }, { 14, kSnareRim },
        };

        // ---------------------------------------------------------------- LO-FI

        constexpr GrooveLane lofiDusty45[] = {
            { 0, "X.......x......." "X.....x.x......." },
            { 2, "........x......." "........x.....x." },
            { 6, "..x...x...x...x." "..x...x...x..xp." },
            { 8, "..............x." "................" },
            { 10, "................" "...p........p..." },
        };
        constexpr GrooveLane lofiTape[] = {
            { 0, "X...x.....x....." "X...x.....x...x." },
            { 2, "....x.......x..." "....x.......x..-" },
            { 6, "x.x.x.x.x.x.x.x." "x.x.x.x.x.xXx.x." },
            { 12, "................" "......p........p" },
            { 14, "-.....-.....-..." "-.....-...-....." },
        };
        constexpr GrooveLane lofiStudy[] = {
            { 0, "X...........x..." "X..............." },
            { 2, "........x......." "........x...-..." },
            { 6, "..-...-...-...-." "..-...-...-..-x." },
            { 8, "................" "x..............." },
            { 10, "................" ".......p........" },
        };
        constexpr GrooveVoice lofiVoicing[] = {
            { 0, kKickRound }, { 2, kSnareBrush }, { 6, kHatClosed },
            { 8, kHatAir }, { 10, kClapSnap },
        };

        // ---------------------------------------------------------------- HARD & CLUB

        constexpr GrooveLane hardSchranz[] = {
            { 0, "X...X...X...X..." "X...X...X...X..x" },
            { 2, "........X......." "........X...x..." },
            { 6, "..x.x.x.x.x.x.x." "..x.x.x.x.x.xXx." },
            { 10, "....X.......X..." "....X...x...X..." },
            { 12, "x..............." "................" },
        };
        constexpr GrooveLane hardBigRoom[] = {
            { 0, "X...X...X...X..." "X...X...X...X..x" },
            { 2, "........X......." "........X...x.x." },
            { 3, "....X.......X..." "....X.......X..f" },
            { 8, "..x...x...x...x." "..x...x...x...xX" },
            { 12, "x..............." "................" },
        };
        constexpr GrooveLane hardHardstyle[] = {
            { 0, "X...X...X...X..." "X...X...X...X..-" },
            { 2, "............X..." "......x.....X..." },
            { 6, "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.xXx." },
            { 8, "..x...x...x...x." "..x...x...x...xx" },
            { 10, "................" "....X..........." },
        };
        constexpr GrooveVoice hardVoicing[] = {
            { 0, kKickPunch }, { 2, kSnareCrack }, { 3, kClapBig },
            { 6, kHatMetal }, { 8, kHatOpen }, { 10, kClapTight },
        };

        // ---------------------------------------------------------------- DISCO

        constexpr GrooveLane discoClassic[] = {
            { 0, "X...X..xX...X..." "X...X..xX...X..x" },
            { 2, "........x......." "........x...x.x." },
            { 3, "................" "....x.......x..." },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-x-xXx-" },
            { 8, "..x...x...x...x." "..x...x...x..Xx." },
            { 12, "..-...-...-...-." "..-...-...-...-x" },
        };
        constexpr GrooveLane discoHustle[] = {
            { 0, "X...X...X...X..." "X...X...X...X..x" },
            { 2, "........x......." "........x.....x." },
            { 6, "x.x.x.x.x.x.x.x." "x.x.x.x.x.xXx.x." },
            { 8, "..x...x...x...x." "..x...x...x...X." },
            { 14, "...>......x...>." "................" },
        };
        constexpr GrooveLane discoFrench[] = {
            { 0, "X...X...X...X..." "X...X...X...X.-x" },
            { 2, "........X......." "......x...X....." },
            { 3, "....x.......x..." "....x.....x.x..." },
            { 6, "..x.x.x...x.x.x." "..x.x.x..x.x.x.x" },
            { 12, "................" "p...........p..." },
        };
        constexpr GrooveVoice discoVoicing[] = {
            { 0, kKickRound }, { 2, kSnareCrack }, { 3, kClapClassic }, { 6, kHatClosed },
            { 8, kHatOpen }, { 12, kHatAir }, { 14, kSnareRim },
        };

        // ---------------------------------------------------------------- TRIP HOP

        constexpr GrooveLane tripDownbeat[] = {
            { 0, "X.....x...x....." "X..>..x...x....>" },
            { 2, "........X......." "......x...X....." },
            { 3, "................" "....-......-...." },
            { 6, "..x...x...x...x." "..x...x...-..xx." },
            { 9, "............x..." "................" },
            { 12, "................" "x..........p...." },
        };
        constexpr GrooveLane tripMidnightRun[] = {
            { 0, "X...x.....x...x." "X...x.....x..xx." },
            { 2, "........x......." "........x...x..." },
            { 4, "..x...x...x...x." "..x...-...-...x." },
            { 8, "x...x...x...x..." "x...x...x...x..-" },
            { 10, "................" "......p....p...." },
        };
        constexpr GrooveLane tripVinylHaze[] = {
            { 0, "X..............." "X.......x......." },
            { 2, "........x...x..." "........x......." },
            { 5, "...-....-....-.." "................" },
            { 6, "..-...-...-...-." "..-...-...-..-x." },
            { 14, "................" "......x.....x..." },
        };
        constexpr GrooveVoice tripVoicing[] = {
            { 0, kKick808 }, { 2, kSnareCrack }, { 6, kHatClosed },
            { 8, kHatOpen }, { 12, kHatAir }, { 14, kSnareRim },
        };

        // ---------------------------------------------------------------- POP

        constexpr GrooveLane popAnthem[] = {
            { 0, "X...X...X...X..." "X...X...X...X..x" },
            { 2, "........X......." "........X...x..." },
            { 3, "....x.......x..." "....x.....x.x..." },
            { 6, "x.x.x.x.x.x.x.x." "x.x.x.x.x.x.xXx." },
            { 8, "..............x." "................" },
            { 10, "................" "......x........." },
        };
        constexpr GrooveLane popSyncopate[] = {
            { 0, "X..x....x..x..x." "X..x....x..x.x.." },
            { 2, "....x.......x..." "....x...x...x..x" },
            { 6, "..x...x...x...x." "..x...x...x...xx" },
            { 11, "x-x-x-x-x-x-x-x-" "x.......x......." },
            { 14, "................" "......>.....>..." },
        };
        constexpr GrooveLane popHalfTime[] = {
            { 0, "X......x........" "X.....x.x......." },
            { 2, "........X......." "........X.....x." },
            { 6, "..x.x.x.x.x.x.x." "..x.x.x.x.x.xXx." },
            { 9, "x.x.xxr.x.x.xxr." "x.x.x.x.x.x.x.x." },
            { 12, "................" "..............p." },
        };
        constexpr GrooveVoice popVoicing[] = {
            { 0, kKickPunch }, { 2, kSnareCrack }, { 3, kClapClassic },
            { 6, kHatClosed }, { 9, kHatOpen }, { 12, kHatAir },
        };

        // ---------------------------------------------------------------- ROCK

        constexpr GrooveLane rockDrive[] = {
            { 0, "X...x...X...x..." "X...x...X...x..x" },
            { 2, "........X......." "........X.....x." },
            { 6, "x...x...x...x..." "x...x...x...x..x" },
            { 8, "......x........." "............x..." },
            { 12, "x..............." "................" },
        };
        constexpr GrooveLane rockBallad[] = {
            { 0, "X..............." "X.......x......." },
            { 2, "........x......." "........x...x..." },
            { 6, "..x...x...x...x." "..x...x...x...xx" },
            { 8, "......x........." "......x.....x..." },
            { 14, "................" "............x.x." },
        };
        constexpr GrooveLane rockStomp[] = {
            { 0, "X..x..X...x..X.." "X..x..X...x..x.x" },
            { 2, "....x.......x..." "....x.......x..r" },
            { 3, "................" "....x.......x..." },
            { 6, "x...x...x...x..." "x...x...x...x..-" },
            { 14, "..-...-...-...-." "..-...-...-...-x" },
        };
        constexpr GrooveVoice rockVoicing[] = {
            { 0, kKickBroken }, { 2, kSnareCrack }, { 6, kHatClosed },
            { 8, kHatOpen }, { 12, kHatAir }, { 14, kSnareRim },
        };

        // ---------------------------------------------------------------- EBM & INDUSTRIAL

        constexpr GrooveLane ebmMartial[] = {
            { 0, "X...X...X...X..." "X...X...X...X..x" },
            { 2, "....x...x...x..x" "....x.......x..." },
            { 4, "..x...x...x...x." "..-...-...-...-." },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-x-xXx-" },
            { 10, "................" "............X..." },
            { 12, "................" "x......p........" },
        };
        constexpr GrooveLane ebmHellbent[] = {
            { 0, "X..X..X..X..X..X" "X..X..X..X..X..x" },
            { 2, "....X.......X..." "....X...X...X..." },
            { 6, "x...x...x...x..." "x...x...x...x..-" },
            { 9, "..x.x.x.x.x.x.x." "..x.x.x.x.x.xXx." },
            { 14, "................" "......X.....X..." },
        };
        constexpr GrooveLane ebmMetalworks[] = {
            { 0, "X...X...X...X..." "X..xX...X...X.x." },
            { 2, "........X...X..." "........X.....x." },
            { 6, "x.x.x.x.x.x.x.x." "x.x.x.xXx.x.x.x." },
            { 9, "..-...-...-...-." "..-...-...-...-x" },
            { 12, "x..............." "................" },
        };
        constexpr GrooveVoice ebmVoicing[] = {
            { 0, kKickBroken }, { 2, kSnareCrack }, { 4, kSnareTight }, { 6, kHatMetal },
            { 9, kHatMetal }, { 10, kClapTight }, { 12, kHatAir }, { 14, kSnareRim },
        };

        // ---------------------------------------------------------------- LATIN

        constexpr GrooveLane latinClave[] = {
            { 0, "X.....x...x....." "X.....x...x....x" },
            { 2, "..x...x..x..x..." "..x...x..x..x..-" },
            { 3, "..x..x....x..x.." "..x..x..x.x..x.." },
            { 6, "x.xx.x.xx.x.xx.x" "x.xx.x.xx.xx.x.x" },
            { 14, "................" "......x....x...." },
        };
        constexpr GrooveLane latinSamba[] = {
            { 0, "X...x...X...x..." "X...x...X...x..x" },
            { 2, "..x.x..x..x.x..x" "..x.x..x..x.x.x." },
            { 6, "x-x-x-x-x-x-x-x-" "x-x-x-x-x-xXx-x-" },
            { 10, "....x.......x..." "....x.....x.x..." },
            { 12, "................" "x......p........" },
        };
        constexpr GrooveLane latinBossa[] = {
            { 0, "X.....x...x....." "X.....x...x....-" },
            { 2, "..x..x...x..x..." "..x..x...x..x..x" },
            { 6, "x...x...x...x..." "x...x...x...x..-" },
            { 9, "..-...-...-...-." "..-...-...-...-x" },
            { 14, "................" "......x.....x..." },
        };
        constexpr GrooveVoice latinVoicing[] = {
            { 0, kKickRound }, { 2, kSnareRim }, { 3, kClapTight }, { 6, kHatClosed },
            { 9, kHatAir }, { 10, kClapSnap }, { 12, kHatAir }, { 14, kSnareRim },
        };

        constexpr Groove kGrooves[] = {
            { "HOUSE", "CLASSIC", 124, 4, 0.00f, houseClassic, houseVoicing },
            { "HOUSE", "DEEP", 122, 4, 0.12f, houseDeep, houseVoicing },
            { "HOUSE", "DRIVE", 126, 4, 0.00f, houseDrive, houseVoicing },
            { "TECHNO", "DRIVE", 132, 4, 0.00f, technoDrive, technoVoicing },
            { "TECHNO", "MACHINE", 138, 4, 0.00f, technoMachine, technoVoicing },
            { "TECHNO", "DUB", 128, 4, 0.00f, technoDub, technoVoicing },
            { "TRAP", "DARK", 140, 4, 0.00f, trapDark, trapVoicing },
            { "TRAP", "ROLLER", 150, 4, 0.00f, trapRoller, trapVoicing },
            { "TRAP", "DRILL", 142, 4, 0.00f, trapDrill, trapVoicing },
            { "BOOM BAP", "DUSTY", 90, 4, 0.22f, boomBapDusty, boomBapVoicing },
            { "BOOM BAP", "JAZZY", 88, 4, 0.18f, boomBapJazzy, boomBapVoicing },
            { "BOOM BAP", "DILLA", 84, 4, 0.10f, boomBapDilla, boomBapVoicing },
            { "DRUM & BASS", "AMEN", 174, 4, 0.00f, dnbAmen, dnbVoicing },
            { "DRUM & BASS", "ROLLER", 172, 4, 0.00f, dnbRoller, dnbVoicing },
            { "DRUM & BASS", "HALFTIME", 170, 4, 0.00f, dnbHalftime, dnbVoicing },
            { "UK GARAGE", "TWO STEP", 132, 4, 0.26f, garageTwoStep, garageVoicing },
            { "UK GARAGE", "SHUFFLE", 130, 4, 0.24f, garageShuffle, garageVoicing },
            { "UK GARAGE", "NIGHTY", 128, 4, 0.22f, garageNighty, garageVoicing },
            { "AFRO", "AFRO 12", 118, 4, 0.30f, afroTwelve, afroVoicing },
            { "AFRO", "AFRO HOUSE", 124, 4, 0.20f, afroHouse, afroVoicing },
            { "AFRO", "AMAPIANO", 112, 4, 0.24f, amapiano, afroVoicing },
            { "DANCEHALL", "DEMBOW", 95, 4, 0.00f, dancehallDembow, dancehallVoicing },
            { "DANCEHALL", "PONY", 100, 4, 0.00f, dancehallPony, dancehallVoicing },
            { "DANCEHALL", "FUSION", 105, 4, 0.00f, dancehallFusion, dancehallVoicing },
            { "BREAKS", "CHOPPA", 135, 4, 0.06f, breaksChoppa, breaksVoicing },
            { "BREAKS", "SKILLED", 140, 4, 0.06f, breaksSkilled, breaksVoicing },
            { "BREAKS", "MINIMAL", 130, 4, 0.00f, breaksMinimal, breaksVoicing },
            { "ELECTRO", "CLASSIC", 128, 4, 0.00f, electroClassic, electroVoicing },
            { "ELECTRO", "OMNI", 130, 4, 0.00f, electroOmni, electroVoicing },
            { "ELECTRO", "RECALL", 126, 4, 0.00f, electroRecall, electroVoicing },
            { "REGGAE & DUB", "ONE DROP", 74, 4, 0.26f, reggaeOneDrop, reggaeVoicing },
            { "REGGAE & DUB", "STEPPERS", 78, 4, 0.18f, reggaeSteppers, reggaeVoicing },
            { "REGGAE & DUB", "DUB PLATE", 70, 4, 0.10f, reggaeDubPlate, reggaeVoicing },
            { "FUNK & SOUL", "FUNKMASTER", 104, 4, 0.06f, funkFunkmaster, funkVoicing },
            { "FUNK & SOUL", "SOUL BALLAD", 88, 4, 0.14f, funkSoulBallad, funkVoicing },
            { "FUNK & SOUL", "SIXTEENTHS", 110, 4, 0.04f, funkSixteenths, funkVoicing },
            { "LO-FI", "DUSTY 45", 74, 4, 0.24f, lofiDusty45, lofiVoicing },
            { "LO-FI", "TAPE", 82, 4, 0.18f, lofiTape, lofiVoicing },
            { "LO-FI", "STUDY", 68, 4, 0.28f, lofiStudy, lofiVoicing },
            { "HARD & CLUB", "SCHRANZ", 146, 4, 0.00f, hardSchranz, hardVoicing },
            { "HARD & CLUB", "BIG ROOM", 128, 4, 0.00f, hardBigRoom, hardVoicing },
            { "HARD & CLUB", "HARDSTYLE", 150, 4, 0.00f, hardHardstyle, hardVoicing },
            { "DISCO", "CLASSIC", 118, 4, 0.06f, discoClassic, discoVoicing },
            { "DISCO", "HUSTLE", 122, 4, 0.00f, discoHustle, discoVoicing },
            { "DISCO", "FRENCH", 124, 4, 0.00f, discoFrench, discoVoicing },
            { "TRIP HOP", "DOWNBEAT", 86, 4, 0.28f, tripDownbeat, tripVoicing },
            { "TRIP HOP", "MIDNIGHT RUN", 92, 4, 0.20f, tripMidnightRun, tripVoicing },
            { "TRIP HOP", "VINYL HAZE", 80, 4, 0.32f, tripVinylHaze, tripVoicing },
            { "POP", "ANTHEM", 120, 4, 0.00f, popAnthem, popVoicing },
            { "POP", "SYNCOPATE", 104, 4, 0.12f, popSyncopate, popVoicing },
            { "POP", "HALF TIME", 140, 4, 0.00f, popHalfTime, popVoicing },
            { "ROCK", "DRIVE", 132, 4, 0.00f, rockDrive, rockVoicing },
            { "ROCK", "BALLAD", 72, 4, 0.16f, rockBallad, rockVoicing },
            { "ROCK", "STOMP", 100, 4, 0.00f, rockStomp, rockVoicing },
            { "EBM & INDUSTRIAL", "MARTIAL", 128, 4, 0.00f, ebmMartial, ebmVoicing },
            { "EBM & INDUSTRIAL", "HELLBENT", 140, 4, 0.00f, ebmHellbent, ebmVoicing },
            { "EBM & INDUSTRIAL", "METALWORKS", 132, 4, 0.00f, ebmMetalworks, ebmVoicing },
            { "LATIN", "CLAVE", 104, 4, 0.10f, latinClave, latinVoicing },
            { "LATIN", "SAMBA", 112, 4, 0.00f, latinSamba, latinVoicing },
            { "LATIN", "BOSSA", 100, 4, 0.16f, latinBossa, latinVoicing },
        };
    } // namespace

    std::span<const Groove> grooveBank()
    {
        return kGrooves;
    }
} // namespace sliceforge
