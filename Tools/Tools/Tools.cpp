#include "./ExcelToLua.h"
#include "Utils.h"
#include <windows.h>

enum EOpenType
{
    EOT_InPathCodeConvert = 1 << 0,
    EOT_SendCommand = 1 << 1
};

int main(int argc, char* argv[]) {
    STDOUT << "参数数量:" << argc << ",编码:" << GetConsoleCP() << STDEND;

    if (argc < 3 || argc > 4) {
        STDERR << "用法: " << argv[0] << " <excel.xlsx> [output_dir] <flag>" << STDEND;
        return -1;
    }
    std::string excelFile;
    std::string outputDir;
    std::uint8_t openFlag = 0;
    if (argc >= 3) {
        excelFile = argv[1];
        outputDir = argv[2];
    }
    if (argc == 4) {
        openFlag = static_cast<std::uint8_t>(std::stoul(argv[3]));
    }
    if (openFlag & EOT_InPathCodeConvert) {
        excelFile = GbkToUtf8(excelFile);
        outputDir = GbkToUtf8(outputDir);
    }
    bool sendcmd = openFlag & EOT_SendCommand;
    ExcelToLua converter;
    converter.SetSymbol(sendcmd);
    if (!converter.Convert(excelFile, outputDir)) {
        STDERR << "转换失败: " << converter.GetLastError() << STDEND;
        return -2;
    }

    STDOUT << "转换成功!" << STDEND;
    return 0;
}
