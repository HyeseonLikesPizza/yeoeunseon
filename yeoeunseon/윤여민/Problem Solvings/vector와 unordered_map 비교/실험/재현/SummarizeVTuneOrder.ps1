# Generated output stays in the study workspace's ignored tmp directory.
$workRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../../tmp'))
if (-not (Test-Path -LiteralPath $workRoot -PathType Container)) { throw 'Build/collect into tmp first.' }
$logText = Get-Content -LiteralPath (Join-Path $workRoot 'VTuneOrderCaptures.log') -Raw
$matches = [regex]::Matches($logText, 'order_profile,(vector|map),(periodic|shuffled),n=8,queries_per_pass=65536,passes=(\d+),elapsed_seconds=([0-9.]+),checksum=(\d+),mismatch=0')
$counts = @{}
$summary = foreach ($m in $matches) {
    $key = $m.Groups[1].Value + '_' + $m.Groups[2].Value
    if (!$counts.ContainsKey($key)) { $counts[$key] = 0 }
    $counts[$key]++
    $capture = $key + '_r' + $counts[$key]
    $eventPath = Join-Path $workRoot ($capture + '_module_events.tsv')
    $hotPath = Join-Path $workRoot ($capture + '_module_hotspots.tsv')
    if (!(Test-Path -LiteralPath $eventPath)) { continue }
    $ev = Get-Content -LiteralPath $eventPath | Where-Object { $_ -notmatch '^war:' } | ConvertFrom-Csv -Delimiter "`t"
    $hot = Get-Content -LiteralPath $hotPath | Where-Object { $_ -notmatch '^war:' } | ConvertFrom-Csv -Delimiter "`t"
    $queries = [double]$m.Groups[3].Value * 65536
    [pscustomobject]@{
        Capture=$capture; Passes=$m.Groups[3].Value; Queries=$queries; Checksum=$m.Groups[5].Value
        NsPerQuery=([double]$m.Groups[4].Value*1e9/$queries)
        InstructionsPerQuery=([double]$ev.'Hardware Event Count:INST_RETIRED.ANY'/$queries)
        CyclesPerQuery=([double]$ev.'Hardware Event Count:CPU_CLK_UNHALTED.THREAD'/$queries)
        BranchMissPerQuery=([double]$ev.'Hardware Event Count:BR_MISP_RETIRED.ALL_BRANCHES'/$queries)
        L1MissPerQuery=([double]$ev.'Hardware Event Count:MEM_LOAD_RETIRED.L1_MISS'/$queries)
        L3MissPerQuery=([double]$ev.'Hardware Event Count:MEM_LOAD_RETIRED.L3_MISS'/$queries)
        Cpi=$hot.'CPI Rate'; FrequencyHz=$hot.'Average CPU Frequency'
        PBranchMispredictPercent=$hot.'Performance-core (P-core):Bad Speculation:Branch Mispredict(%)'
        EBranchMispredictPercent=$hot.'Efficient-core (E-core):Bad Speculation:Branch Mispredict(%)'
        PMemoryBoundPercent=$hot.'Performance-core (P-core):Back-End Bound:Memory Bound(%)'
    }
}
$summary | Export-Csv -LiteralPath (Join-Path $workRoot 'VTuneOrderSummary.csv') -NoTypeInformation
$summary | ConvertTo-Json -Compress
