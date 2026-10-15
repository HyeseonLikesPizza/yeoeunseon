param([int]$StartAt = 0)
# Generated output stays in the study workspace's ignored tmp directory.
$workRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../../tmp'))
if (-not (Test-Path -LiteralPath $workRoot -PathType Container)) { throw 'Build/collect into tmp first.' }
$vtuneExe = 'C:/Program Files (x86)/Intel/oneAPI/vtune/2026.4/bin64/vtune.exe'
$captures = @('aos256_r1','ids4_r1','ids4_r2','aos256_r2')
foreach ($capture in ($captures | Select-Object -Skip $StartAt)) {
    $resultPath = Join-Path $workRoot ('vtune_split_' + $capture)
    & $vtuneExe -finalize -r $resultPath -search-dir $workRoot *> (Join-Path $workRoot ($capture + '_split_resolve.log'))
    if ($LASTEXITCODE -ne 0) { throw "Finalize failed: $capture" }
    & $vtuneExe -report hotspots -r $resultPath -group-by module -filter 'module=vectorsplitprofile.exe' -format csv -report-knob show-issues=false -report-output (Join-Path $workRoot ($capture + '_split_hotspots.tsv')) 2>$null
    if ($LASTEXITCODE -ne 0) { throw "Hotspots failed: $capture" }
    & $vtuneExe -report hw-events -r $resultPath -group-by module -filter 'module=vectorsplitprofile.exe' -format csv -column 'INST_RETIRED.ANY,CPU_CLK_UNHALTED.THREAD,BR_MISP_RETIRED.ALL_BRANCHES,MEM_LOAD_RETIRED.L1_MISS,MEM_LOAD_RETIRED.L2_HIT,MEM_LOAD_RETIRED.L3_HIT,MEM_LOAD_RETIRED.L3_MISS,DTLB_LOAD_MISSES.WALK_ACTIVE,MEMORY_ACTIVITY.STALLS_L1D_MISS' -report-output (Join-Path $workRoot ($capture + '_split_events.tsv')) 2>$null
    if ($LASTEXITCODE -ne 0) { throw "Events failed: $capture" }
    & $vtuneExe -report hotspots -r $resultPath -format csv -limit 5 -report-knob show-issues=false -report-output (Join-Path $workRoot ($capture + '_split_functions.tsv')) 2>$null
    if ($LASTEXITCODE -ne 0) { throw "Functions failed: $capture" }
    & $vtuneExe -report summary -r $resultPath -report-knob show-issues=false -report-output (Join-Path $workRoot ($capture + '_split_summary.txt')) 2>$null
    if ($LASTEXITCODE -ne 0) { throw "Summary failed: $capture" }
    Write-Output "Split reports ready: $capture"
}
