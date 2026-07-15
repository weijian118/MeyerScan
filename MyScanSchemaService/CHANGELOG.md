# 修改记录

## 2026-07-15

- 新建 `MeyerScan_ScanSchemaService.dll` 和 `ScanSchemaServiceTest.exe`。
- 将种植体、临时牙、异性扫描杆、扫描杆分段、咬合类型到扫描步骤的生成规则集中到服务层。
- 输出稳定步骤编码和 `labelKey`，不在跨模块 JSON 中保存翻译后的界面文本。
- 增加 API 版本、代码版本、固定结果结构和缓冲区不足测试。
