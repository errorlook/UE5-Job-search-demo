return {
    Settings = {
        {
            SettingId = "Graphics.Quality",
            DisplayName = "画质",
            Description = "调整游戏整体画质等级。",
            Category = "Video",
            Options = {
                { Label = "低", Value = 0 },
                { Label = "中", Value = 1 },
                { Label = "高", Value = 2 },
                { Label = "史诗", Value = 3 },
            },
            Visible = true,
            SortOrder = 10,
        },
    },
    Text = {
        PageTitle = "设置",
        ApplyButton = "应用并保存",
        ResetButton = "恢复默认",
        VideoTab = "画面",
        AudioTab = "声音",
        KeysTab = "按键",
    },
}
