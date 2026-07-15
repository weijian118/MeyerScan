# MyScanSchemaService

`MeyerScan_ScanSchemaService.dll` 负责把建单页收集的扫描配置和牙位方案转换为标准 `scanProcess.steps`。

## 边界

- 输入为 UTF-8 JSON，输出为调用方缓冲区中的 UTF-8 JSON。
- 不创建 UI，不访问数据库，不控制设备，不执行扫描算法。
- 步骤合同只保存稳定 `code/labelKey`，显示文本由各 UI 使用 `tr()` 生成。
- 新增扫描流程规则优先修改本模块和测试，不在 OrderCreateUI、MainExe、ScanWorkflowUI 中复制判断。
