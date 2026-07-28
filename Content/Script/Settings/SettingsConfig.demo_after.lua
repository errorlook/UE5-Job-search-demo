-- Copy this table into SettingsConfig.lua during the recorded demonstration.
return {
    Settings = {
        {
            SettingId = "Graphics.Quality",
            DisplayName = "整体画面质量",
            Description = "调整场景、阴影、特效与后处理的综合质量。",
            Category = "Video",
            Options = {
                { Label = "流畅", Value = 0 },
                { Label = "均衡", Value = 1 },
                { Label = "精致", Value = 2 },
                { Label = "极致", Value = 3 },
            },
            Visible = true,
            SortOrder = 10,
        },
    },
    Text = {
        PageTitle = "游戏设置",
        ApplyButton = "应用并保存",
        ResetButton = "恢复默认",
        VideoTab = "显示",
        AudioTab = "音频",
        KeysTab = "控制",
    },
}
