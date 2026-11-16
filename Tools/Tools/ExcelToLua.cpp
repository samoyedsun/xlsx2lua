#include "ExcelToLua.h"
#include <sstream>
#include <regex>
#include <filesystem>
#include <set>
#include <utility>
#include <cmath>

namespace fs = std::filesystem;

static std::string cellToString(const OpenXLSX::XLCellValue& cellVal) {
    if (cellVal.type() == OpenXLSX::XLValueType::Empty) {
        return "";
    }
    switch (cellVal.type()) {
    case OpenXLSX::XLValueType::Boolean:
        return cellVal.get<bool>() ? "true" : "false";
    case OpenXLSX::XLValueType::Integer:
        return std::to_string(cellVal.get<int64_t>());
    case OpenXLSX::XLValueType::Float:
    {
        double val = cellVal.get<double>();
        if (std::floor(val) == val) {
            return std::to_string(static_cast<int64_t>(val));
        }
        std::ostringstream oss;
        oss << val;
        return oss.str();
    }
    case OpenXLSX::XLValueType::String:
        return cellVal.get<std::string>();
    default:
        return "";
    }
}

ConfigWriter::ConfigWriter(const std::string& tableName, OutPutType type)
    : m_tableName(tableName)
    , m_type(type)
    , m_canOutPutTable(true)
    , m_breakNum(0) {
}

bool ConfigWriter::canCheck(size_t index) const {
    if (index >= m_keyNameList.size() || index >= m_outputType.size() || index >= m_outputGroup.size()) {
        return false;
    }
    if (m_keyNameList[index].empty() || m_outputType[index].empty() || m_outputGroup[index].empty()) {
        return false;
    }
    return true;
}

bool ConfigWriter::canDo(size_t colIndex) const {
    if (canCheck(colIndex)) {
        return !m_keyNameList[colIndex].empty() &&
            (std::stoi(m_outputType[colIndex]) == static_cast<int>(OutPutType::All) ||
                std::stoi(m_outputType[colIndex]) == static_cast<int>(m_type));
    }
    return false;
}

void ConfigWriter::checkKey(const std::string& key) {
    if (m_indexKey.find(key) != m_indexKey.end()) {
        throw std::runtime_error("表头重复: " + m_tableName + ", 含有重复的键: " + key);
    } else {
        m_indexKey[key] = 1;
    }
}

void ConfigWriter::checkHead() {
    int keyColIndex = -1;
    std::string exceptStr = m_tableName + " 表头异常 请确定:\n第一行应为描述\n第二行应为字段名\n第三行应为导出类型\n第四行应为导出目标";

    if (m_keyNameList.empty() || m_descList.empty() || m_outputType.empty() || m_outputGroup.empty()) {
        throw std::runtime_error(exceptStr);
    }

    for (size_t index = 0; index < m_outputGroup.size(); ++index) {
        try {
            int groupVal = std::stoi(m_outputGroup[index]);
            if (groupVal == static_cast<int>(OutPutGroupType::KeyNumber) ||
                groupVal == static_cast<int>(OutPutGroupType::KeyString)) {
                keyColIndex = static_cast<int>(index);
            }

            if (canCheck(index)) {
                if (groupVal < static_cast<int>(OutPutGroupType::Null) ||
                    groupVal > static_cast<int>(OutPutGroupType::TableOrNull)) {
                    throw std::runtime_error(exceptStr);
                }
            }
        } catch (...) {
        }
    }

    for (size_t index = 0; index < m_outputType.size(); ++index) {
        try {
            int typeVal = std::stoi(m_outputType[index]);
            if (canCheck(index)) {
                if (typeVal < static_cast<int>(OutPutType::All) ||
                    typeVal > static_cast<int>(OutPutType::Server)) {
                    throw std::runtime_error(exceptStr);
                }
            }
        } catch (...) {
        }
    }

    for (size_t i = 0; i < m_keyNameList.size() - 1; ++i) {
        for (size_t j = i + 1; j < m_keyNameList.size(); ++j) {
            if (canCheck(i)) {
                if (!m_keyNameList[i].empty() && m_keyNameList[i] == m_keyNameList[j]) {
                    throw std::runtime_error("不能出现重复的导出字段 " + m_keyNameList[i]);
                }
            }
        }
    }

    if (keyColIndex >= 0 && !canDo(static_cast<size_t>(keyColIndex))) {
        m_canOutPutTable = false;
    }
}

void ConfigWriter::initHead() {
    if (!m_canOutPutTable) {
        return;
    }

    m_fd << "-- " + m_tableName + "  ";
    for (size_t index = 0; index < m_keyNameList.size(); ++index) {
        if (canDo(index)) {
            m_fd << " " + m_keyNameList[index] + ":" + m_descList[index] + " ";
        }
    }
    m_fd << "\n";
    m_fd << "local " + m_tableName + " = {\n";
    addBreak();
}

void ConfigWriter::open(const std::vector<std::string>& keyNameList,
    const std::vector<std::string>& descList,
    const std::vector<std::string>& outputGroup,
    const std::vector<std::string>& outputType) {
    m_keyNameList = keyNameList;
    m_descList = descList;
    m_outputGroup = outputGroup;
    m_outputType = outputType;
    checkHead();
    initHead();
}

void ConfigWriter::addBreak() {
    m_breakNum = m_breakNum + 1;
}

std::string ConfigWriter::getLineBreak() const {
    std::string breakStr;
    for (int num = 0; num < m_breakNum; ++num) {
        breakStr += "\t";
    }
    return breakStr;
}

std::string ConfigWriter::formatKeyValue(const std::vector<std::string>& lineValues, size_t colIndex) const {
    if (!canDo(colIndex)) {
        return "";
    }
    std::string key = m_keyNameList[colIndex];
    std::string value = (colIndex < lineValues.size()) ? lineValues[colIndex] : "";
    int valueType = 0;
    try {
        valueType = std::stoi(m_outputGroup[colIndex]);
    } catch (...) {
        return "";
    }

    auto isNone = [](const std::string& v) {
        return v.empty();
    };

    auto toNumString = [](const std::string& v) -> std::string {
        if (v.empty()) return "0";
        try {
            double dval = std::stod(v);
            if (std::floor(dval) == dval) {
                return std::to_string(static_cast<int64_t>(dval));
            }
            std::ostringstream oss;
            oss << dval;
            return oss.str();
        } catch (...) {
            return v;
        }
    };

    if (isNone(value)) {
        if (valueType == static_cast<int>(OutPutGroupType::NumberOrNull) ||
            valueType == static_cast<int>(OutPutGroupType::StringOrNull) ||
            valueType == static_cast<int>(OutPutGroupType::TableOrNull)) {
            return "";
        }
        if (valueType == static_cast<int>(OutPutGroupType::Number) ||
            valueType == static_cast<int>(OutPutGroupType::KeyNumber)) {
            return key + " = 0,";
        }
        if (valueType == static_cast<int>(OutPutGroupType::String) ||
            valueType == static_cast<int>(OutPutGroupType::KeyString)) {
            return key + " = [[]],";
        }
        if (valueType == static_cast<int>(OutPutGroupType::Table)) {
            return key + " = {},";
        }
        if (valueType == static_cast<int>(OutPutGroupType::Native)) {
            return ",";
        }
    }

    if (valueType == static_cast<int>(OutPutGroupType::Number) ||
        valueType == static_cast<int>(OutPutGroupType::KeyNumber) ||
        valueType == static_cast<int>(OutPutGroupType::NumberOrNull) ||
        valueType == static_cast<int>(OutPutGroupType::Native)) {
        value = toNumString(value);
        return key + " = " + value + ",";
    } else if (valueType == static_cast<int>(OutPutGroupType::String) ||
        valueType == static_cast<int>(OutPutGroupType::KeyString) ||
        valueType == static_cast<int>(OutPutGroupType::StringOrNull)) {
        return key + " = [[" + value + "]],";
    } else {
        return key + " = " + value + ",";
    }
}

void ConfigWriter::writeLine(const std::vector<std::string>& lineValues) {
    if (!m_canOutPutTable) {
        return;
    }

    std::string firstValue = lineValues.empty() ? "" : lineValues[0];
    if (firstValue.empty()) {
        return;
    }

    auto isNone = [](const std::string& v) { return v.empty(); };

    auto toNumString = [](const std::string& v) -> std::string {
        if (v.empty()) return "0";
        try {
            double dval = std::stod(v);
            if (std::floor(dval) == dval) {
                return std::to_string(static_cast<int64_t>(dval));
            }
            std::ostringstream oss;
            oss << dval;
            return oss.str();
        } catch (...) {
            return v;
        }
    };

    int group0 = 0;
    try {
        if (!m_outputGroup.empty() && !m_outputGroup[0].empty()) {
            group0 = std::stoi(m_outputGroup[0]);
        }
    } catch (...) {
        group0 = 0;
    }

    if (!m_outputGroup.empty() && !m_outputGroup[0].empty()) {
        if (group0 == static_cast<int>(OutPutGroupType::KeyNumber)) {
            if (isNone(firstValue)) {
                return;
            }
            checkKey(firstValue);
            m_fd << getLineBreak() + "[" + toNumString(firstValue) + "] = {\n";
            addBreak();
        } else if (group0 == static_cast<int>(OutPutGroupType::KeyString)) {
            if (isNone(firstValue)) {
                return;
            }
            checkKey(firstValue);
            m_fd << getLineBreak() + "[\"" + firstValue + "\"] = {\n";
            addBreak();
        } else {
            m_fd << getLineBreak() + "{\n";
            addBreak();
        }
    } else {
        m_fd << getLineBreak() + "{\n";
        addBreak();
    }

    for (size_t colIndex = 0; colIndex < m_keyNameList.size(); ++colIndex) {
        std::string keyValueStr = formatKeyValue(lineValues, colIndex);
        if (!keyValueStr.empty()) {
            m_fd << getLineBreak() + keyValueStr + "\n";
        }
    }

    m_breakNum = 1;
    m_fd << getLineBreak() + "},\n";
}

std::string ConfigWriter::close() {
    if (!m_canOutPutTable) {
        return "";
    }
    m_fd << "}\n";
    return m_fd.str();
}

ExcelToLua::ExcelToLua() {
}

void ExcelToLua::SetSymbol(bool sendcmd) {
    m_sendCommand = sendcmd;
}

bool ExcelToLua::isNum(const std::string& str) const {
    if (str.empty()) return false;
    try {
        std::stoi(str);
        return true;
    } catch (...) {
        return false;
    }
}

std::string ExcelToLua::converInt(const std::string& value) const {
    if (isNum(value)) {
        try {
            double dval = std::stod(value);
            if (dval == static_cast<int64_t>(dval)) {
                return std::to_string(static_cast<int64_t>(dval));
            }
        } catch (...) {
        }
    }
    return value;
}

bool ExcelToLua::isNone(const std::string& value) const {
    return value.empty();
}

std::string ExcelToLua::toString(const std::string& value) const {
    if (value.empty()) {
        return "";
    }
    return converInt(value);
}

double ExcelToLua::toFloat(const std::string& value) const {
    if (value.empty()) {
        return 0;
    }
    double val = 0;
    try {
        val = std::stod(value);
    } catch (...) {
        STDERR << "value = " << value << STDEND;
        return 0;
    }
    val = val * 100000 + 0.1;
    if (val > 0) {
        val = static_cast<int64_t>(val + 0.1) / 100000.0;
    }
    if (val < 0) {
        val = static_cast<int64_t>(val - 0.1) / 100000.0;
    }
    return val;
}

double ExcelToLua::toInt(const std::string& value) const {
    if (value.empty()) {
        return 0;
    }
    try {
        std::stod(value);
    } catch (...) {
        STDERR << "value = " << value << STDEND;
    }
    double intValue = static_cast<int64_t>(std::stod(value));
    double floatValue = toFloat(value);
    if (intValue != floatValue) {
        return floatValue;
    }
    return intValue;
}

std::string ExcelToLua::toNumString(const std::string& value) const {
    if (isNone(value)) {
        return "0";
    }
    try {
        double dval = std::stod(value);
        double intValue = toInt(value);
        std::ostringstream oss;
        oss << intValue;
        return oss.str();
    } catch (...) {
        return converInt(value);
    }
}

std::string ExcelToLua::getTableName(const std::string& sheetName) const {
    std::regex pattern("\\((.*?)\\)");
    std::smatch match;
    if (std::regex_search(sheetName, match, pattern)) {
        std::string result = match[1].str();
        std::string cleaned;
        for (char c : result) {
            if (c != '[' && c != ']' && c != '(' && c != ')') {
                cleaned += c;
            }
        }
        return cleaned;
    }
    return "";
}

static void ensureDir(const std::string& path) {
    fs::path dirPath = fs::u8path(path);
    if (!fs::exists(dirPath)) {
        fs::create_directories(dirPath);
    }
}

void ExcelToLua::analyzeExcel(OpenXLSX::XLWorksheet& ws, const std::string& tableName,
    std::vector<LuaTableOutput>& clientTables, std::vector<LuaTableOutput>& serverTables) {
    size_t nrows = ws.rowCount();
    if (nrows <= 4) {
        STDOUT << "表少于4行无法导出" << STDEND;
        return;
    }

    size_t ncols = ws.columnCount();

    auto getRowValues = [&](size_t rowIndex) -> std::vector<std::string> {
        std::vector<std::string> values;
        values.reserve(ncols);
        auto row = ws.row(rowIndex);
        for (size_t col = 1; col <= ncols; ++col) {
            auto cell = row.findCell(col);
            if (cell) {
                values.push_back(cellToString(cell.value()));
            } else {
                values.push_back("");
            }
        }
        return values;
    };

    std::vector<std::string> descList = getRowValues(1);
    std::vector<std::string> keyNameList = getRowValues(2);
    std::vector<std::string> outputGroup = getRowValues(3);
    std::vector<std::string> outputType = getRowValues(4);

    auto getGroupNum = [&]() -> int {
        int num = 0;
        for (size_t index = 0; index < outputGroup.size(); ++index) {
            if (!keyNameList[index].empty() && !outputType[index].empty() && !outputGroup[index].empty()) {
                num++;
            }
        }
        return num;
    };

    auto getGroupNumByType = [&](int oType) -> int {
        int num = 0;
        for (size_t index = 0; index < outputType.size(); ++index) {
            try {
                if (std::stoi(outputType[index]) == oType) {
                    num++;
                }
            } catch (...) {
            }
        }
        return num;
    };

    bool justClient = false;
    bool justServer = false;

    int totalGroup = getGroupNum();
    if (getGroupNumByType(static_cast<int>(OutPutType::Client)) == totalGroup) {
        justClient = true;
    }
    if (getGroupNumByType(static_cast<int>(OutPutType::Server)) == totalGroup) {
        justServer = true;
    }

    // 客户端、服务端共用一次行遍历，避免重复读取以及进度重复上报
    std::vector<std::pair<OutPutType, ConfigWriter>> writers;
    writers.reserve(2);
    if (!justServer) {
        writers.emplace_back(OutPutType::Client, ConfigWriter(tableName, OutPutType::Client));
    }
    if (!justClient) {
        writers.emplace_back(OutPutType::Server, ConfigWriter(tableName, OutPutType::Server));
    }

    for (auto& writer : writers) {
        writer.second.open(keyNameList, descList, outputGroup, outputType);
    }

    for (size_t rowIndex = 5; rowIndex <= nrows; ++rowIndex) {
        std::vector<std::string> lineValues = getRowValues(rowIndex);
        for (auto& writer : writers) {
            writer.second.writeLine(lineValues);
        }

        if (m_sendCommand) {
            STDCMD << "@progress(" << rowIndex << "," << nrows << ")" << STDEND;
        }
    }

    for (auto& writer : writers) {
        std::string text = writer.second.close();
        if (text.empty()) {
            continue;
        }

        LuaTableOutput table{ tableName, std::move(text) };
        if (writer.first == OutPutType::Server) {
            serverTables.push_back(std::move(table));
        } else {
            clientTables.push_back(std::move(table));
        }
    }
}

static void writeLuaTables(const std::string& filePath, const std::vector<LuaTableOutput>& tables) {
    std::string text;
    for (const auto& table : tables) {
        text += table.text;
        text += "\n";
    }

    text += "return {\n";
    for (const auto& table : tables) {
        text += "\t" + table.name + " = " + table.name + ",\n";
    }
    text += "}\n";

    // 内容统一从 UTF-8 转成 GB2312(GBK) 后写入
    std::string gbkText = Utf8ToGbk(text);
    std::vector<uint8_t> data(gbkText.begin(), gbkText.end());
    if (!WriteFile(filePath, data)) {
        throw std::runtime_error("无法打开文件: " + filePath);
    }
}

bool ExcelToLua::Convert(const std::string& excelPath, const std::string& outputPath) {
    if (m_sendCommand) {
        auto processId = GetProcessId();
        STDCMD << "@processid(" << processId << ")" << STDEND;
    }
    try {
        STDOUT << "打开配置表:" << excelPath << STDEND;

        std::string basePath = outputPath.empty() ? "." : outputPath;
        std::string clientPath = basePath + "/client";
        std::string serverPath = basePath + "/server";

        ensureDir(basePath);
        ensureDir(clientPath);
        ensureDir(serverPath);

        OpenXLSX::XLDocument doc(excelPath);
        auto workbook = doc.workbook();
        auto sheetNames = workbook.sheetNames();

        // lua文件名优先取Excel文件名括号中的部分，如 B_捕鱼房间(FishRoom).xlsx -> FishRoom.lua
        std::string excelName = GetFilenameWithoutExt(excelPath);
        std::string luaFileName = getTableName(excelName);
        if (luaFileName.empty()) {
            luaFileName = excelName;
        }
        std::vector<LuaTableOutput> clientTables;
        std::vector<LuaTableOutput> serverTables;
        std::set<std::string> tableNames;

        for (const auto& sheetName : sheetNames) {
            std::string tableName = getTableName(sheetName);
            if (tableName.empty()) {
                continue;
            }
            if (!tableNames.insert(tableName).second) {
                throw std::runtime_error("表名重复: 同一Excel中出现了多个 " + tableName + " 表");
            }

            auto ws = workbook.worksheet(sheetName);
            analyzeExcel(ws, tableName, clientTables, serverTables);
        }

        if (!clientTables.empty()) {
            writeLuaTables(clientPath + "/" + luaFileName + ".lua", clientTables);
        }
        if (!serverTables.empty()) {
            writeLuaTables(serverPath + "/" + luaFileName + ".lua", serverTables);
        }

        return true;
    } catch (const std::exception& e) {
        m_lastError = "转换失败: " + std::string(e.what());
        return false;
    }
}
