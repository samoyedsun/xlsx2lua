#ifndef INCLUDE_EXCELTOLUA_HPP_
#define INCLUDE_EXCELTOLUA_HPP_

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <OpenXLSX/OpenXLSX.hpp>
#include "./Utils.h"

enum class OutPutGroupType {
    Null = 0,
    Number = 1,
    String = 2,
    Table = 3,
    KeyNumber = 4,
    KeyString = 5,
    Native = 6,
    NumberOrNull = 7,
    StringOrNull = 8,
    TableOrNull = 9,
};

enum class OutPutType {
    All = 0,
    Client = 1,
    Server = 2,
};

class ConfigWriter {
public:
    ConfigWriter(const std::string& tableName, OutPutType type);

    void open(const std::vector<std::string>& keyNameList,
        const std::vector<std::string>& descList,
        const std::vector<std::string>& outputGroup,
        const std::vector<std::string>& outputType);

    void writeLine(const std::vector<std::string>& lineValues);
    // 生成单个lua表的文本，该表不导出时返回空串
    std::string close();

    bool canOutPutTable() const { return m_canOutPutTable; }

private:
    bool canDo(size_t colIndex) const;
    bool canCheck(size_t index) const;
    void initHead();
    void checkHead();
    void checkKey(const std::string& key);
    void addBreak();
    std::string getLineBreak() const;
    std::string formatKeyValue(const std::vector<std::string>& lineValues, size_t colIndex) const;

    std::string m_tableName;
    OutPutType m_type;
    std::map<std::string, int> m_indexKey;
    bool m_canOutPutTable;
    std::ostringstream m_fd;
    int m_breakNum;

    std::vector<std::string> m_keyNameList;
    std::vector<std::string> m_descList;
    std::vector<std::string> m_outputGroup;
    std::vector<std::string> m_outputType;
};

// 一个lua表的名称与生成内容
struct LuaTableOutput {
    std::string name;
    std::string text;
};

class ExcelToLua {
public:
    ExcelToLua();

    void SetSymbol(bool sendcmd);

    bool Convert(const std::string& excelPath, const std::string& outputPath);

    std::string GetLastError() const { return m_lastError; }

private:
    bool isNum(const std::string& str) const;
    std::string toString(const std::string& value) const;
    std::string toNumString(const std::string& value) const;
    std::string converInt(const std::string& value) const;
    bool isNone(const std::string& value) const;
    double toFloat(const std::string& value) const;
    double toInt(const std::string& value) const;

    std::string getTableName(const std::string& sheetName) const;
    void analyzeExcel(OpenXLSX::XLWorksheet& ws, const std::string& tableName,
        std::vector<LuaTableOutput>& clientTables, std::vector<LuaTableOutput>& serverTables);

    std::string m_lastError;
    std::string m_excelPath;
    bool m_sendCommand = false;
};

#endif
