#!/usr/bin/env bash
# ────────────────────────────────────────────────────────────────
#  道·码 vs Python  对比实验
#  用法：cd daocode && bash duibi/run.sh
# ────────────────────────────────────────────────────────────────
set -uo pipefail
cd "$(dirname "$0")/.."

CLPSD="./clpsd"
PY="python3"

bar()  { printf '%.0s━' {1..60}; echo; }
thin() { printf '%.0s─' {1..60}; echo; }

lines() { wc -l < "$1" | tr -d ' '; }

# 运行并计时（ms）
run_timed() {
    local label="$1"; shift
    local t0 t1 out rc=0
    t0=$(python3 -c "import time; print(int(time.perf_counter()*1000))")
    out=$("$@" 2>&1) || rc=$?
    t1=$(python3 -c "import time; print(int(time.perf_counter()*1000))")
    echo "$out"
    printf "  ⏱  %dms\n" "$((t1 - t0))"
    return $rc
}

compare() {
    local title="$1" pyf="$2" daof="$3"
    bar
    printf "  %s\n" "$title"
    printf "  Python %d行  |  道·码 %d行\n" "$(lines "$pyf")" "$(lines "$daof")"
    thin
    printf "▶ Python:\n"
    run_timed "py" $PY "$pyf"
    echo ""
    printf "▶ 道·码:\n"
    run_timed "dao" $CLPSD "$daof"
    echo ""
}

# ────────────────────────────────────────────────────────────────
# ① 基础·变量+运算+输出
# ────────────────────────────────────────────────────────────────
compare "① 基础·变量运算输出" \
    duibi/01_jichu/test.py \
    duibi/01_jichu/test.dao

# ────────────────────────────────────────────────────────────────
# ② 循环求和
# ────────────────────────────────────────────────────────────────
compare "② 循环求和（小5次·大1000次）" \
    duibi/02_xunhuan/test.py \
    duibi/02_xunhuan/test.dao

# ────────────────────────────────────────────────────────────────
# ③ 条件分支
# ────────────────────────────────────────────────────────────────
compare "③ 条件分支·if vs 艮" \
    duibi/03_tiaojian/test.py \
    duibi/03_tiaojian/test.dao

# ────────────────────────────────────────────────────────────────
# ④ 类型守卫（道·码三级防御）
# ────────────────────────────────────────────────────────────────
bar
echo "  ④ 类型守卫·Python动态 vs 道·码五行三级防御"
printf "  Python %d行  |  道·码坎 %d行  |  道·码震 %d行\n" \
    "$(lines duibi/04_leixing/test.py)" \
    "$(lines duibi/04_leixing/test_kan.dao)" \
    "$(lines duibi/04_leixing/test_zhen.dao)"
thin
echo "▶ Python（运行时才发现错误）:"
run_timed "py" $PY duibi/04_leixing/test.py
echo ""
echo "▶ 道·码 坎卦（安全模式·克制静默·程序继续）:"
run_timed "kan" $CLPSD duibi/04_leixing/test_kan.dao
echo ""
echo "▶ 道·码 震卦（严格模式·需卦编译期阻止·不等运行时）:"
$CLPSD --check duibi/04_leixing/test_zhen.dao 2>&1 || true
echo "  ↑ 编译时拒绝，Python 要到运行时才知道"
echo ""

# ────────────────────────────────────────────────────────────────
# ⑤ 错误恢复
# ────────────────────────────────────────────────────────────────
compare "⑤ 错误恢复·try/except vs 蹇化" \
    duibi/05_cuowu/test.py \
    duibi/05_cuowu/test.dao

bar
echo "  实验完毕"
bar
