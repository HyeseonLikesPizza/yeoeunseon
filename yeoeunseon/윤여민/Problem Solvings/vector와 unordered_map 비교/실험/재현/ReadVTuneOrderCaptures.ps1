# Generated output stays in the study workspace's ignored tmp directory.
$workRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../../tmp'))
if (-not (Test-Path -LiteralPath $workRoot -PathType Container)) { throw 'Build/collect into tmp first.' }
$vtuneExe = 'C:/Program Files (x86)/Intel/oneAPI/vtune/2026.4/bin64/vtune.exe'
$outputRoot = $workRoot
$captures = @(
    'vector_periodic_r1', 'map_periodic_r1', 'vector_shuffled_r1', 'map_shuffled_r1',
    'map_shuffled_r2', 'vector_shuffled_r2', 'map_periodic_r2', 'vector_periodic_r2'
)
foreach ($capture in $captures) {
    $resultPath = Join-Path $outputRoot ('vtune_order_' + $capture)
    # Re-resolve our matching EXE/PDB with a Unicode-aware PowerShell argument.
    & $vtuneExe -finalize -r $resultPath -search-dir $outputRoot *> (Join-Path $outputRoot ($capture + '_resolve.log'))
    if ($LASTEXITCODE -ne 0) { throw "Finalize failed: $capture" }
    & $vtuneExe -report hotspots -r $resultPath -format csv -report-knob show-issues=false -limit 5 -report-output (Join-Path $outputRoot ($capture + '_hotspots.tsv')) 2>$null
    if ($LASTEXITCODE -ne 0) { throw "Hotspots report failed: $capture" }
    & $vtuneExe -report hw-events -r $resultPath -format csv -column 'INST_RETIRED.ANY,CPU_CLK_UNHALTED.THREAD,BR_INST_RETIRED.ALL_BRANCHES,BR_MISP_RETIRED.ALL_BRANCHES,MEM_LOAD_RETIRED.L1_MISS,MEM_LOAD_RETIRED.L3_MISS' -limit 5 -report-output (Join-Path $outputRoot ($capture + '_events.tsv')) 2>$null
    if ($LASTEXITCODE -ne 0) { throw "Events report failed: $capture" }
    $hot = Get-Content -LiteralPath (Join-Path $outputRoot ($capture + '_hotspots.tsv')) | Where-Object { $_ -notmatch '^war:' } | ConvertFrom-Csv -Delimiter "`t" | Where-Object { $_.Function -match '^(VectorLookup|MapLookup)' } | Select-Object -First 1
    $events = Get-Content -LiteralPath (Join-Path $outputRoot ($capture + '_events.tsv')) | Where-Object { $_ -notmatch '^war:' } | ConvertFrom-Csv -Delimiter "`t" | Where-Object { $_.Function -match '^(VectorLookup|MapLookup)' } | Select-Object -First 1
    if (!$hot -or !$events) { throw "Lookup symbol missing: $capture" }
    [pscustomobject]@{
        Capture = $capture; Function = $hot.Function; CpuSeconds = $hot.'CPU Time'
        Instructions = $hot.'Instructions Retired'; Cycles = $hot.Clockticks; Cpi = $hot.'CPI Rate'
        PBranchMispredictPercent = $hot.'Performance-core (P-core):Bad Speculation:Branch Mispredict(%)'
        EBranchMispredictPercent = $hot.'Efficient-core (E-core):Bad Speculation:Branch Mispredict(%)'
        PMemoryBoundPercent = $hot.'Performance-core (P-core):Back-End Bound:Memory Bound(%)'
        Frequency = $hot.'Average CPU Frequency'
        Branches = $events.'Hardware Event Count:BR_INST_RETIRED.ALL_BRANCHES'
        Mispredictions = $events.'Hardware Event Count:BR_MISP_RETIRED.ALL_BRANCHES'
        L1Misses = $events.'Hardware Event Count:MEM_LOAD_RETIRED.L1_MISS'
        L3Misses = $events.'Hardware Event Count:MEM_LOAD_RETIRED.L3_MISS'
    } | ConvertTo-Json -Compress
}
