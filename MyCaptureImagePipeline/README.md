# MyCaptureImagePipeline

`MeyerScan_CaptureImagePipeline.dll` 位于协议级 `CaptureProcessing` 与 UI/三维重建算法之间，负责场景级图像再处理和多路结果发布。

当前已实现：

- 从标准化黑/R/G/B/激光G/激光B 六图生成 RGB888 显示图。
- 为重建链路提供标准化六图深副本。
- 通过 `outputType`、`optionsRevision`、`appliedFeatures` 和 `unavailableFeatures` 描述每一路结果。
- 请求尚未接入的颜色校准、AI 软组织、除色或粗条纹必需功能时明确返回 `FeatureUnavailable`，并清空旧结果。

后续算法 DLL 只能由本模块适配；CaptureService 和 UI 不直接加载具体算法。构建入口为 `MeyerScan_CaptureImagePipeline.sln` 或 CMake `vs2015-x64` 预设，测试为 `CaptureImagePipelineTest.exe --smoke`。
