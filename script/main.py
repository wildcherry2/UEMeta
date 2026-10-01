import logging
from pathlib import Path

from upg.write_compile_commands.write_compile_commands import filter_compile_commands
from utility import CONFIG, generate_proto, invoke_parser_cc

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    generate_proto()
    filter_compile_commands(Path(r"D:\intermediate\UnrealEngine\compile_commands.json"))
    invoke_parser_cc(Path(r"D:\UEMeta\parser\out\parser.exe"), "5.7", Path(r"D:\intermediate\UnrealEngine\compile_commands.json"),
                     enable_unreal_extensions=True,
                     log_level="info",
                     format="json",
                     output=Path(r"D:\intermediate\ParserOutput"),
                     additional_clang_args=["-resource-dir=D:/UEMeta/parser/intermediate/deps/src/"
                                            "clang+llvm-23.1.0-x86_64-pc-windows-msvc/lib/clang/23"])
