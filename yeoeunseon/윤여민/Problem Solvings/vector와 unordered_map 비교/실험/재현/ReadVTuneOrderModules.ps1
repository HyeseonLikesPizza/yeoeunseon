# Generated output stays in the study workspace's ignored tmp directory.
$workRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../../tmp'))
if (-not (Test-Path -LiteralPath $workRoot -PathType Container)) { throw 'Build/collect into tmp first.' }
$vtuneExe = 'C:/Program Files (x86)/Intel/oneAPI/vtune/2026.4/bin64/vtune.exe'
$captures = @('vector_periodic_r1','map_periodic_r1','vector_shuffled_r1','map_shuffled_r1','map_shuffled_r2','vector_shuffled_r2','map_periodic_r2','vector_periodic_r2')
foreach ($capture in $captures) {
    $resultPath = Join-Path $workRoot ('vtune_order_' + $capture)
    $hotPath = Join-Path $workRoot ($capture + '_module_hotspots.tsv')
    $eventPath = Join-Path $workRoot ($capture + '_module_events.tsv')
    # Module scope includes std::unordered_map::find, which was not inlined here.
    & $vtuneExe -report hotspots -r $resultPath -group-by module -filter 'module=vectororderprofile.exe' -format csv -report-knob show-issues=false -report-output $hotPath 2>$null
    if ($LASTEXITCODE -ne 0) { throw "Module hotspots failed: $capture" }
    & $vtuneExe -report hw-events -r $resultPath -group-by module -filter 'module=vectororderprofile.exe' -format csv -column 'INST_RETIRED.ANY,CPU_CLK_UNHALTED.THREAD,BR_INST_RETIRED.ALL_BRANCHES,BR_MISP_RETIRED.ALL_BRANCHES,MEM_LOAD_RETIRED.L1_MISS,MEM_LOAD_RETIRED.L3_MISS' -report-output $eventPath 2>$null
    if ($LASTEXITCODE -ne 0) { throw "Module events failed: $capture" }
    Write-Output "Module reports ready: $capture"
}
