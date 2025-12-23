setup vscode

. .venv/bin/activate

(cd config; esphome compile ping4pow.yaml)

mkdir -p .vscode
cat <<EOF >.vscode/settings
{
    "C_Cpp.default.compileCommands": "${workspaceFolder:config}/.esphome/build/ping4pow/compile_commands.json",
    "C_Cpp.intelliSenseEngine": "default",
    "C_Cpp.indexing.maxCachedFileSize": 100
}
EOF


