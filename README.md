# African Drum - by African Gospel

Sampler de bateria (VST3 / AU / Standalone) para macOS e Windows, feito com JUCE 8.
**Vem sem samples de bateria**: carregas os teus proprios samples e kits. Inclui a biblioteca MIDI de grooves.

## Interface (4 abas)

- **Performance** (layout inspirado na caixa do produto): **KIT MIXER** (abas TONE / SPACE: EQ, saturacao, compressor,
  master, room, delay, largura, humanize), a bateria 3D ao centro (clica num tambor para tocar; Shift = hi-hat aberto / side stick,
  Alt = kick 2; clica no sino do ride ou no aro da caixa) com o seletor de kit no topo, **PAD SHAPER** (afinacao, decay, pan,
  nivel, mute/solo/gate, LOAD/CLEAR do pad selecionado), **KIT PRESETS** (lista dos teus kits), lista **DRUM PADS**
  (duplo clique = carregar sample), **GROOVE PLAYER** (genero, groove, secoes, piano-roll) e **PERCUSSION MIXER**
  (faders de grupo: kick, snare, toms, hats, cymbals).
- **MIDI MAP** (botao no canto superior direito): mapeia as notas que chegam da tua bateria eletronica ou teclado.
  Menu: *- Empty -*, *AD2 Standard*, *General MIDI*, *African Drum Default (C1-G#3)*, e marcas (2box, Alesis, Efnote, GEWA,
  MegaDRUM, Millenium, Nux, Other plugins, Roland, Simmons, Yamaha) com os modelos. Os mapas seguem as disposicoes de
  fabrica mais comuns (GM e "Standard e-kit": T1-T4 = 48/45/43/41, rim 40, cross-stick 37, hats 42/44/46, crash 49/57,
  ride 51, bell 53). Se algum pad de um modulo especifico nao bater, usa **MIDI Learn**: escolhe o pad, abre o menu,
  *MIDI Learn*, e toca o pad do kit eletronico. A escolha fica gravada no projeto. Os grooves internos e os MIDIs
  arrastados para a DAW usam sempre o mapa nativo (C1-G#3).
- **Mixer**: as 16 strips completas (nivel, pan, pitch, decay, mute, solo, saida, choke, gate, medidor) + FX.
- **Effects**: cadeia de efeitos no Main - Equalizador de 3 bandas (com curva), Compressor (com medidor de reducao),
  Saturacao, Delay ping-pong, Reverb de sala (tamanho, damping, mix), Largura estereo, Humanize e Master.
  Cada efeito liga/desliga e todos os parametros sao automatizaveis na DAW.
- **Groove**: botao **IMPORT MIDI** (ou arrasta .mid / pastas para a aba): os teus MIDIs ficam em `Documentos/African Gospel/African Drum/MIDI Import` e aparecem no genero **My MIDI**. **GM MAP** converte as notas General MIDI (38 snare, 42 hi-hat...) para os pads do plugin. Biblioteca MIDI completa em listas (Genero > Groove > Secao), com pesquisa, audicao por clique,
  visao piano-roll, arrastar o MIDI para a timeline e EXPORT ALL.

## O que vem incluido

- **Sem samples de fabrica**: os 16 pads arrancam vazios. Carrega samples em cada pad (arrastar, `LOAD` ou menu do pad),
  ou cria kits completos na pasta de kits. Troca de kit a qualquer momento (setas ou lista), mesmo a tocar.
- **Os teus kits** (DW, Sonor, Mapex, Tama...): cria uma pasta por kit em
  `Documentos/African Gospel/African Drum/Kits/<nome do kit>/` com os teus samples.
  O botao **MY KITS FOLDER** abre essa pasta; o kit aparece na lista em poucos segundos.
  Os nomes sao reconhecidos (kick, snare, rim, side stick, tom floor/low/mid/high, hat closed/pedal/open,
  crash, ride, bell); sem nomes reconheciveis, os ficheiros vao por ordem alfabetica para os pads 1-16.
- **Samples externos**: WAV, AIFF, FLAC, OGG, MP3. Arrasta para um pad, usa o menu do pad, ou **IMPORT FOLDER**.
- **Mixer de 16 canais**: nivel, pan, pitch, decay, mute, solo, medidor, saida, choke, gate. Master + room reverb + humanize.
- **16 saidas estereo** (Main + Out 2..16).
- **Biblioteca MIDI** (grooves em 16 generos (Gospel: 171 em 3/4, 4/4, 6/8 e 12/8; Naija Praise: 75) + fills): Gospel, Naija Praise, Seben (o teu MIDI), Worship, Rock, Blues, Pop, Funk, Soul, Jazz,
  Latin, Reggae, Country, Hip-Hop, Metal, Dance, Fills. Cada groove tem
  **Full Song, Intro, Verse, Pre-Chorus, Chorus, Bridge, Break, Outro, Fill A/B/C, Groove**.
  Toca dentro do plugin (loop, segue o BPM da DAW) ou **arrasta o MIDI para a timeline** (DRAG MIDI TO DAW).
  **EXPORT ALL** grava os 3526 ficheiros .mid numa pasta.

Notas MIDI: 36-56 (C1 a G#3). Kick 36, Kick 2 37, Snare 38, Rim 39, Side stick 40, Floor tom 41, Low 42, Mid 43,
High 44, HH closed 45, HH pedal 46, HH open 47, Crash 48, Crash 49, Ride 50, Bell 51.

## Compilar

Requisitos: CMake 3.22+ e
- macOS: Xcode (Command Line Tools)
- Windows: Visual Studio 2022 (C++ desktop workload)

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Resultado em `build/DrumForge_artefacts/Release/` (`VST3`, `AU` no Mac, `Standalone`).
Instalar: VST3 em `C:\Program Files\Common Files\VST3` (Windows) ou `/Library/Audio/Plug-Ins/VST3` (Mac);
AU em `/Library/Audio/Plug-Ins/Components`.

Sem instalar nada: faz push do projeto para o GitHub; o workflow `.github/workflows/build.yml` compila Mac e Windows,
valida com pluginval e disponibiliza os plugins como ficheiros para descarregar.

## Compatibilidade

- **macOS**: binario universal (Apple Silicon + Intel), alvo minimo macOS 10.13. Formatos VST3 e AU
  (Logic, GarageBand, Ableton, Reaper, Cubase, Studio One, Bitwig, FL Studio, Digital Performer...).
- **Windows**: x64, runtime C++ estatico (nao precisa do Visual C++ Redistributable), VST3.
  Compatibilidade com Windows 7/8 nao e garantida com JUCE 8 (pede Windows 10+); para isso seria preciso JUCE 7.
- **Pro Tools (AAX)** exige o SDK e assinatura da Avid. **iPhone/iPad** exigiria uma versao AUv3 separada. Nenhum dos dois esta incluido.
- **Saidas multiplas**: ative as saidas extra na DAW (Reaper: Routing > Pin; Ableton: "+" ao lado do plugin;
  Logic: "+" na faixa; FL Studio: Plugin wrapper > Sidechain/Outputs). Se uma saida nao estiver ativa, o pad toca na Main.

## Notas

- Nao ha samples incluidos. `Tools/DemoKit` tem sons sinteticos usados so na demo (nao entram no plugin). Os MIDIs sao padroes
  originais (`Tools/generate_midi.py`) mais o MIDI Seben fornecido.
- `Tools/Demo/make_demo.py` gera o video tutorial (como adicionar samples, kits e MIDIs) a partir do plugin real.
- `Tools/Snapshot` e uma ferramenta de desenvolvimento que renderiza a interface real para PNG/video.
- Licenca JUCE: para distribuir/vender e preciso AGPLv3 ou licenca comercial JUCE.
- No macOS e preciso assinar e notarizar o plugin para distribuir.


## Instalador
Ver `Installer/INSTALL.md`: instaladores Windows (`.exe`, Inno Setup) e macOS (`.pkg`/`.dmg`) gerados pelo GitHub Actions ou localmente com `Installer/build_mac.sh` / `Installer/build_windows.bat`.

## MIDI MAP padrão
O mapa padrão agora é **General MIDI** (universal). Para usar as notas nativas (C1–G#3) escolha *African Drum Default* no botão MIDI MAP. Os grooves internos usam sempre o mapa nativo e não são afetados.

## New in this version
- **Velocity layers + Sensitivity** (mixer deck): expressive velocity curve, soft hits are quieter and darker. Cymbals have soft/medium/hard layers.
- **Cymbal microphones**: four faders (Dry / Studio / Room / Hall) mixed together in the mixer.
- **Hi-hat CC4** pedal: closed / half-open / open.
- **TUNER** on every mixer channel: detected note + cents, fine tuning in 1-cent steps.
- **Mixer presets**: 5 factory presets per kit (Natural, Tight and Dry, Live Room, Gospel Worship, Big Hall) + SAVE/LOAD MIX.
- **Balanced sample levels** across all kits.
- **Licence** (optional): build with `-DAG_REQUIRE_LICENSE=ON -DAG_LICENSE_SECRET=...`; keys from `Tools/License/keygen.py`. See TESTING.md.
