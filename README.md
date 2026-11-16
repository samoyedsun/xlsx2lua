# xlsx2lua

将 Excel 配置表导出为 Lua 文件的工具。一个 Excel 对应一个 Lua 文件，文件内包含该 Excel 中所有表。

## 使用方式

1. 将 Excel 放到 `ConfigDirectory` 目录下，文件名按 `编号_名称(导出文件名).xlsx` 命名，例如 `B_捕鱼房间(FishRoom).xlsx`。括号中的 `FishRoom` 就是导出的 Lua 文件名；文件名不带括号时直接使用整个文件名。
2. 在 `ConfigDirectory` 目录下执行 `ExcelToBytes.bat`，参数为要导出的 Excel：

   ```bat
   .\ExcelToBytes.bat .\B_捕鱼房间(FishRoom).xlsx
   ```

   也可以把 Excel 文件直接拖到 `ExcelToBytes.bat` 上。第二个参数可以指定输出目录，默认为当前目录。
3. 导出结果：

   ```text
   ConfigDirectory/client/FishRoom.lua
   ConfigDirectory/server/FishRoom.lua
   ```

   - 一个 Excel 的所有表都写在同一个文件里，每个表是 `local 表名 = { ... }`，文件末尾返回所有表：

     ```lua
     local config = require("FishRoom")
     -- config.room_gun_level_config、config.catch_fish_pool_ratio_conf ...
     ```

   - `client` 只包含导出到客户端的表，`server` 只包含导出到服务端的表。某一端没有可导出的表时不会生成该文件。
   - 生成的 Lua 文件内容为 GB2312（GBK）编码。

## 表格格式

每个 sheet 的前四行是表头，从第五行开始是数据：

| 行 | 内容 |
| --- | --- |
| 第 1 行 | 字段描述 |
| 第 2 行 | 字段名（不填则该列不导出，不能重复） |
| 第 3 行 | 字段类型（1~9） |
| 第 4 行 | 导出方式（0~2） |
| 第 5 行起 | 数据 |

- 只有名称中带 `(表名)` 的 sheet 才会导出，例如 `任务配置(task_config)` 导出为 `task_config` 表。同一 Excel 内表名不能重复。
- 第一列必须是 key（字段类型 4 或 5）。
- 数据行第一列为空时整行跳过；sheet 不足 5 行时没有数据，不会导出。

### 导出方式（第 4 行）

| 值 | 含义 |
| --- | --- |
| 0 | 客户端、服务端都导出 |
| 1 | 只导出客户端 |
| 2 | 只导出服务端 |

若所有导出列的导出方式都是 1，则只生成客户端文件；都是 2 则只生成服务端文件。

### 字段类型（第 3 行）

| 值 | 含义 |
| --- | --- |
| 1 | 数字，不填默认 0 |
| 2 | 字符串，不填默认 `""` |
| 3 | 表，不填默认 `{}` |
| 4 | 数字 key，不填则整行不导出 |
| 5 | 字符串 key，不填则整行不导出 |
| 6 | 原样导出（填什么导出什么） |
| 7 | 数字，不填则不导出该字段 |
| 8 | 字符串，不填则不导出该字段 |
| 9 | 表，不填则不导出该字段 |

## 目录结构

```text
ConfigDirectory/        Excel 源表与导出工具
    ExcelToBytes.bat    导出脚本
    Tools.exe           导出工具
    client              客户端 Lua 配置
    server              服务端 Lua 配置
Tools/                  Tools.exe 的 C++ 源码（Tools.sln）
```

## 编译

用 Visual Studio 2022 打开 `Tools/Tools.sln`，按 Release x64 编译。依赖 `$(SVR_Header_2D)\sdk\OpenXLSX-0.5.1` 和 `$(SVR_Header_2D)\sdk\boost_1_79_0`（用于 `boost::nowide` 处理 UTF-8 路径）。
