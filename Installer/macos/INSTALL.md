# African Drum – Instalação e teste

## Opção A – Instaladores prontos (GitHub Actions, sem compilar nada)
1. Crie um repositório no GitHub e envie esta pasta (`git init && git add . && git commit -m dbl && git push`).
2. Separador **Actions** → workflow **Build** corre sozinho (ou "Run workflow").
3. No fim, em **Artifacts** descarregue:
   - `African-Drum-macOS-Installer` → `African-Drum-1.0.0-macOS.dmg` / `.pkg`
   - `African-Drum-Windows-Installer` → `African-Drum-Setup-1.0.0-Windows.exe`
4. Para publicar uma versão: `git tag v1.0.0 && git push --tags` (cria um Release com os instaladores).

## Opção B – Compilar no seu computador
- **Mac:** `brew install cmake` → `./Installer/build_mac.sh` → resultado em `Installer/output/`.
- **Windows:** instale Visual Studio 2022 (Desktop C++), CMake e Inno Setup 6 → `Installer\build_windows.bat` → `Installer\output\`.

## Instalar
**macOS** – abra o `.dmg`, duplo clique no `.pkg`. O instalador não está assinado com Apple Developer ID, por isso o macOS pode bloquear:
clique com o botão direito no `.pkg` → **Abrir** → Abrir; ou *Definições do Sistema → Privacidade e Segurança → "Abrir mesmo assim"*.
Locais: VST3 `/Library/Audio/Plug-Ins/VST3`, AU `/Library/Audio/Plug-Ins/Components`, app em `/Applications`.
No Logic, se o AU não aparecer, correr uma vez `auval -v aumu Agd1 Agsp` ou reiniciar o Mac.

**Windows** – execute `African-Drum-Setup-1.0.0-Windows.exe`. O SmartScreen pode avisar "Editor desconhecido": **Mais informações → Executar mesmo assim**.
VST3 em `C:\Program Files\Common Files\VST3`. Reinicie a DAW e faça re-scan.

## Testar
1. Abra a DAW, crie uma pista de instrumento e carregue **African Drum** (VST3, ou AU no Logic).
2. Separador GROOVE → escolha um género → PLAY; separador PERFORMANCE → troque o kit.
3. Teste o Standalone para ver a interface sem DAW.
4. Kits e MIDIs próprios: `Documentos/African Gospel/African Drum/Kits` e `MIDI Import`.

## Desinstalar
Windows: Definições → Aplicações → African Drum. Mac: apague os 3 itens listados acima.

## Assinatura (opcional, remove os avisos)
Mac: conta Apple Developer → variáveis `DEV_ID_APP`, `DEV_ID_INSTALLER`, `NOTARY_PROFILE` (ver `build_pkg.sh`).
Windows: assine o `.exe` com `signtool` e um certificado de code signing.
