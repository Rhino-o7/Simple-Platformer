function Clean-Line([string]$line) {
    if ($null -eq $line) { return "" }
    $i = $line.IndexOf('#')
    if ($i -ge 0) { $line = $line.Substring(0, $i) }
    return $line.Trim()
}

function Get-Tokens([string]$line) {
    $clean = Clean-Line $line
    if ([string]::IsNullOrWhiteSpace($clean)) { return @() }
    $matches = [regex]::Matches($clean, '"([^"]*)"|(\S+)')
    $tokens = @()
    foreach ($m in $matches) {
        $t = if ($m.Groups[1].Success) { $m.Groups[1].Value } else { $m.Groups[2].Value }
        $tokens += $t
    }
    return ,$tokens
}

function Parse-Vec([string]$line, [int]$n) {
    $clean = Clean-Line $line
    $parts = @($clean -split '\s+' | Where-Object { $_ -ne '' })
    if ($parts.Count -lt $n) { throw "Expected $n numbers in '$line'" }
    $vals = @()
    for ($i=0; $i -lt $n; $i++) { $vals += [double]$parts[$i] }
    return $vals
}

function Parse-Num([string]$line) {
    $clean = Clean-Line $line
    $parts = @($clean -split '\s+' | Where-Object { $_ -ne '' })
    if ($parts.Count -lt 1) { throw "Expected number in '$line'" }
    return [double]$parts[0]
}

$files = Get-ChildItem Client/data/scene -Filter *.scn | Where-Object { $_.Name -ne 'main.scn' }
foreach ($file in $files) {
    Write-Output ("Converting " + $file.Name)
    $lines = Get-Content $file.FullName
    $idx = 0
    $entities = @()

    while ($idx -lt $lines.Count) {
        $clean = Clean-Line $lines[$idx]
        if ($clean.StartsWith('$')) { break }
        $idx++
    }

    while ($idx -lt $lines.Count) {
        $line = Clean-Line $lines[$idx]
        if ([string]::IsNullOrWhiteSpace($line)) { $idx++; continue }
        if (-not $line.StartsWith('$')) { $idx++; continue }

        $entityId = $line.Substring(1)
        $idx++
        while ($idx -lt $lines.Count -and [string]::IsNullOrWhiteSpace((Clean-Line $lines[$idx]))) { $idx++ }
        if ($idx -ge $lines.Count) { break }

        $compCount = [int](Get-Tokens $lines[$idx])[0]
        $idx++

        $components = [ordered]@{}
        for ($c = 0; $c -lt $compCount; $c++) {
            while ($idx -lt $lines.Count -and [string]::IsNullOrWhiteSpace((Clean-Line $lines[$idx]))) { $idx++ }
            $tokens = Get-Tokens $lines[$idx]
            if ($tokens.Count -eq 0) { $idx++; continue }
            $comp = $tokens[0]
            $idx++

            switch ($comp) {
                'Transform' {
                    $parentLine = Clean-Line $lines[$idx]; $idx++
                    $parent = $null
                    if ($parentLine.StartsWith('$')) {
                        $p = $parentLine.Substring(1).Trim()
                        if ($p -ne 'Null') { $parent = $p }
                    }
                    $position = Parse-Vec $lines[$idx] 3; $idx++
                    $scale = Parse-Vec $lines[$idx] 3; $idx++
                    $rotation = Parse-Vec $lines[$idx] 4; $idx++
                    $t = [ordered]@{ position = $position; scale = $scale; rotation = $rotation }
                    if ($null -ne $parent) { $t.parent = $parent }
                    $components.transform = $t
                }
                'Collider' {
                    $body = $tokens[1]
                    $shape = $tokens[2]
                    $col = [ordered]@{ body = $body; shape = $shape }
                    if ($shape -eq 'AABB') {
                        $col.min = Parse-Vec $lines[$idx] 3; $idx++
                        $col.max = Parse-Vec $lines[$idx] 3; $idx++
                    } elseif ($shape -eq 'Sphere') {
                        $col.radius = Parse-Num $lines[$idx]; $idx++
                    }
                    $components.collider = $col
                }
                'Renderable' {
                    if ($tokens.Count -ge 3 -and $tokens[1] -eq 'Model') {
                        $components.renderable = [ordered]@{ model = $tokens[2] }
                    }
                }
                'Behaviour' {
                    $type = $tokens[1]
                    $b = [ordered]@{ type = $type }
                    switch ($type) {
                        'Platform' {
                            $b.from = Parse-Vec $lines[$idx] 3; $idx++
                            $b.to = Parse-Vec $lines[$idx] 3; $idx++
                            $b.speed = Parse-Num $lines[$idx]; $idx++
                        }
                        'Turret' {
                            $bulletTokens = Get-Tokens $lines[$idx]; $idx++
                            $b.bulletAsset = $bulletTokens[0]
                            $b.delay = Parse-Num $lines[$idx]; $idx++
                            $b.speed = Parse-Num $lines[$idx]; $idx++
                        }
                        'Firetrap' {
                            $assets = Get-Tokens $lines[$idx]; $idx++
                            $b.smokeAsset = $assets[0]
                            $b.firespreadAsset = $assets[1]
                            $b.recoil = Parse-Num $lines[$idx]; $idx++
                            $b.delay = Parse-Num $lines[$idx]; $idx++
                            $b.time = Parse-Num $lines[$idx]; $idx++
                            $b.center = Parse-Vec $lines[$idx] 3; $idx++
                            if ($idx -lt $lines.Count) { $b.seed = Parse-Num $lines[$idx]; $idx++ }
                        }
                        'Firespread' {
                            $assets = Get-Tokens $lines[$idx]; $idx++
                            $b.smokeAsset = $assets[0]
                            $b.recoil = Parse-Num $lines[$idx]; $idx++
                            $b.delay = Parse-Num $lines[$idx]; $idx++
                            $b.time = Parse-Num $lines[$idx]; $idx++
                            $b.center = Parse-Vec $lines[$idx] 3; $idx++
                            if ($idx -lt $lines.Count) { $b.seed = Parse-Num $lines[$idx]; $idx++ }
                        }
                        'Smoke' {
                            $b.center = Parse-Vec $lines[$idx] 3; $idx++
                            $b.speed = Parse-Num $lines[$idx]; $idx++
                        }
                        'PlayerController' {
                            $refs = Get-Tokens $lines[$idx]; $idx++
                            $b.torso = $refs[0].Substring(1)
                            $b.lfoot = $refs[1].Substring(1)
                            $b.rfoot = $refs[2].Substring(1)
                            $b.lhand = $refs[3].Substring(1)
                            $b.rhand = $refs[4].Substring(1)
                            $feet = Get-Tokens $lines[$idx]; $idx++
                            $b.feetCollider = $feet[0].Substring(1)
                        }
                        default {
                        }
                    }
                    $components.behaviour = $b
                }
                'Light' {
                    $type = $tokens[1]
                    $l = [ordered]@{ type = $type }
                    if ($type -eq 'Point') {
                        $att = Parse-Vec $lines[$idx] 3; $idx++
                        $l.constant = $att[0]
                        $l.linear = $att[1]
                        $l.quadratic = $att[2]
                    }
                    $l.ambient = Parse-Vec $lines[$idx] 3; $idx++
                    $l.diffuse = Parse-Vec $lines[$idx] 3; $idx++
                    $components.light = $l
                }
                'Camera' {
                    $cam = [ordered]@{}
                    $cam.fov = Parse-Num $lines[$idx]; $idx++
                    $cam.zNear = Parse-Num $lines[$idx]; $idx++
                    $cam.zFar = Parse-Num $lines[$idx]; $idx++
                    $components.camera = $cam
                }
                default {
                    throw "Unsupported component '$comp' in $($file.Name)"
                }
            }
        }

        $entities += [ordered]@{ id = $entityId; components = $components }
    }

    $out = [ordered]@{ name = [System.IO.Path]::GetFileNameWithoutExtension($file.Name); entities = $entities }
    $json = $out | ConvertTo-Json -Depth 16
    $outPath = [System.IO.Path]::ChangeExtension($file.FullName, '.json')
    Set-Content -Path $outPath -Value $json -Encoding utf8
}
