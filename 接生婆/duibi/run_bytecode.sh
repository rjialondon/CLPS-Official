#!/usr/bin/env bash
# ────────────────────────────────────────────────────────────────
#  Python  vs  道·码·字节码VM  对比实验
#  用法：cd daocode && bash duibi/run_bytecode.sh
# ────────────────────────────────────────────────────────────────
set -uo pipefail
cd "$(dirname "$0")/.."

CLPSD="./clpsd"
PY="python3"

bar()  { printf '%.0s━' {1..60}; echo; }
thin() { printf '%.0s─' {1..60}; echo; }

compare() {
    local title="$1" pyf="$2" clpsf="$3"
    bar
    printf "  %s\n" "$title"
    thin
    printf "▶ Python:\n"
    $PY "$pyf"
    printf "▶ 道·码·字节码VM:\n"
    $CLPSD --bytecode "$clpsf"
    echo ""
}

# ① 基础
compare "① 基础·变量运算输出" \
    duibi/01_jichu/test.py \
    duibi/01_jichu/test.clps

# ② 循环
compare "② 循环求和" \
    duibi/02_xunhuan/test.py \
    duibi/02_xunhuan/test.clps

# ③ 条件
compare "③ 条件分支" \
    duibi/03_tiaojian/test.py \
    duibi/03_tiaojian/test.clps

# ④ 类型守卫（坎）
bar
echo "  ④ 类型守卫·坎（安全模式）"
thin
printf "▶ Python（运行时错误，x 不变）:\n"
$PY duibi/04_leixing/test.py
printf "▶ 道·码·字节码VM（坎·静默，x 不变）:\n"
$CLPSD --bytecode duibi/04_leixing/test_kan.clps 2>/dev/null
echo ""

# ⑤ 蹇化
compare "⑤ 错误恢复·蹇化" \
    duibi/05_cuowu/test.py \
    duibi/05_cuowu/test.clps

bar
echo "  对比完毕"
bar
