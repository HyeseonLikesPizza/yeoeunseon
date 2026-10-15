# Generated output stays in the study workspace's ignored tmp directory.
$workRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../../tmp'))
if (-not (Test-Path -LiteralPath $workRoot -PathType Container)) { throw 'Build/collect into tmp first.' }
$text = Get-Content -LiteralPath (Join-Path $workRoot 'VTuneCoverageCaptures.log') -Raw
$matches = [regex]::Matches($text, 'coverage_profile,active=(512|50000),n=50000,queries_per_pass=(\d+),unique=(\d+),passes=(\d+),elapsed_seconds=([0-9.]+),checksum=(\d+),equalities_per_query=([0-9.]+),buckets=(\d+),load=([0-9.]+),cpu=(\d+),mismatch=0')
$counts = @{}
$summary = foreach ($m in $matches) {
    $key = $m.Groups[1].Value
    if (!$counts.ContainsKey($key)) { $counts[$key]=0 }; $counts[$key]++
    $capture = $key + '_r' + $counts[$key]
    $ep = Join-Path $workRoot ($capture + '_coverage_events.tsv')
    $hp = Join-Path $workRoot ($capture + '_coverage_hotspots.tsv')
    if (!(Test-Path -LiteralPath $ep)) { continue }
    $ev = Get-Content -LiteralPath $ep | Where-Object { $_ -notmatch '^war:' } | ConvertFrom-Csv -Delimiter "`t"
    $hot = Get-Content -LiteralPath $hp | Where-Object { $_ -notmatch '^war:' } | ConvertFrom-Csv -Delimiter "`t"
    $queries = [double]$m.Groups[2].Value * [double]$m.Groups[4].Value
    [pscustomobject]@{
        Capture=$capture; Passes=$m.Groups[4].Value; Queries=$queries; Checksum=$m.Groups[6].Value
        Unique=$m.Groups[3].Value; EqualitiesPerQuery=$m.Groups[7].Value; Cpu=$m.Groups[10].Value
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
        Cpi=$hot.'CPI Rate'; FrequencyHz=$hot.'Average CPU Frequency'
        PCycles=$hot.'Clockticks:Performance-core (P-core)'; ECycles=$hot.'Clockticks:Efficient-core (E-core)'
        PMemoryBoundPercent=$hot.'Performance-core (P-core):Back-End Bound:Memory Bound(%)'
        PL3BoundPercent=$hot.'Performance-core (P-core):Back-End Bound:Memory Bound:L3 Bound(%)'
    }
}
$summary | Export-Csv -LiteralPath (Join-Path $workRoot 'VTuneCoverageSummary.csv') -NoTypeInformation
$summary | ConvertTo-Json -Compress
