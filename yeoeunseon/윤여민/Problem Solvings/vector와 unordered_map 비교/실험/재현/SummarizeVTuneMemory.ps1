# Generated output stays in the study workspace's ignored tmp directory.
$workRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../../tmp'))
if (-not (Test-Path -LiteralPath $workRoot -PathType Container)) { throw 'Build/collect into tmp first.' }
$text = Get-Content -LiteralPath (Join-Path $workRoot 'VTuneMemoryCaptures.log') -Raw
$matches = [regex]::Matches($text, 'profile,(vector|map),(8|256),50000,queries_per_pass=(\d+),passes=(\d+),elapsed_seconds=([0-9.]+),checksum=(\d+),mismatch=0')
$counts = @{}
$summary = foreach ($m in $matches) {
    $key = $m.Groups[1].Value + '_' + $m.Groups[2].Value
    if (!$counts.ContainsKey($key)) { $counts[$key]=0 }; $counts[$key]++
    $capture = $key + '_r' + $counts[$key]
    $ep = Join-Path $workRoot ($capture + '_memory_events.tsv')
    $hp = Join-Path $workRoot ($capture + '_memory_hotspots.tsv')
    if (!(Test-Path -LiteralPath $ep)) { continue }
    $ev = Get-Content -LiteralPath $ep | Where-Object { $_ -notmatch '^war:' } | ConvertFrom-Csv -Delimiter "`t"
    $hot = Get-Content -LiteralPath $hp | Where-Object { $_ -notmatch '^war:' } | ConvertFrom-Csv -Delimiter "`t"
    $queries = [double]$m.Groups[3].Value * [double]$m.Groups[4].Value
    [pscustomobject]@{
        Capture=$capture; Passes=$m.Groups[4].Value; Queries=$queries; Checksum=$m.Groups[6].Value
        NsPerQuery=([double]$m.Groups[5].Value*1e9/$queries)
        InstructionsPerQuery=([double]$ev.'Hardware Event Count:INST_RETIRED.ANY'/$queries)
        CyclesPerQuery=([double]$ev.'Hardware Event Count:CPU_CLK_UNHALTED.THREAD'/$queries)
        BranchMissPerQuery=([double]$ev.'Hardware Event Count:BR_MISP_RETIRED.ALL_BRANCHES'/$queries)
        L1MissPerQuery=([double]$ev.'Hardware Event Count:MEM_LOAD_RETIRED.L1_MISS'/$queries)
        L2HitPerQuery=([double]$ev.'Hardware Event Count:MEM_LOAD_RETIRED.L2_HIT'/$queries)
        L3HitPerQuery=([double]$ev.'Hardware Event Count:MEM_LOAD_RETIRED.L3_HIT'/$queries)
        L3MissPerQuery=([double]$ev.'Hardware Event Count:MEM_LOAD_RETIRED.L3_MISS'/$queries)
        DtlbWalkCyclesPerQuery=([double]$ev.'Hardware Event Count:DTLB_LOAD_MISSES.WALK_ACTIVE'/$queries)
        L1MissStallCyclesPerQuery=([double]$ev.'Hardware Event Count:MEMORY_ACTIVITY.STALLS_L1D_MISS'/$queries)
        L2MissStallCyclesPerQuery=([double]$ev.'Hardware Event Count:MEMORY_ACTIVITY.STALLS_L2_MISS'/$queries)
        L3MissStallCyclesPerQuery=([double]$ev.'Hardware Event Count:MEMORY_ACTIVITY.STALLS_L3_MISS'/$queries)
        Cpi=$hot.'CPI Rate'; FrequencyHz=$hot.'Average CPU Frequency'
        PCycles=$hot.'Clockticks:Performance-core (P-core)'; ECycles=$hot.'Clockticks:Efficient-core (E-core)'
        PMemoryBoundPercent=$hot.'Performance-core (P-core):Back-End Bound:Memory Bound(%)'
        PL1BoundPercent=$hot.'Performance-core (P-core):Back-End Bound:Memory Bound:L1 Bound(%)'
        PL2BoundPercent=$hot.'Performance-core (P-core):Back-End Bound:Memory Bound:L2 Bound(%)'
        PL3BoundPercent=$hot.'Performance-core (P-core):Back-End Bound:Memory Bound:L3 Bound(%)'
        PDramBoundPercent=$hot.'Performance-core (P-core):Back-End Bound:Memory Bound:DRAM Bound(%)'
    }
}
$summary | Export-Csv -LiteralPath (Join-Path $workRoot 'VTuneMemorySummary.csv') -NoTypeInformation
$summary | ConvertTo-Json -Compress
