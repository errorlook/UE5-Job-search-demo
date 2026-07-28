-- Copy these entries into QuestConfig.lua during the recorded demonstration.
return {
    {
        QuestId = "Boss_Aurora",
        Title = "肃清哥布林营地",
        Description = "深入营地并击败所有负责警戒的守卫。",
        ObjectiveText = "击败 5 个营地守卫",
        TargetCount = 5,
        SortOrder = 10,
        RewardText = "解锁新角色",
        Visible = true,
        CanAccept = true,
    },
    {
        QuestId = "Quest.FindCrystal",
        Title = "寻找寒冰水晶",
        Description = "调查营地北侧的遗迹，寻找散发寒气的水晶。",
        ObjectiveText = "找到寒冰水晶",
        TargetCount = 1,
        SortOrder = 20,
        RewardText = "神秘奖励",
        Visible = true,
        -- Display-only until a native UQuestDataAsset supplies validation/rewards.
        CanAccept = false,
    },
}
