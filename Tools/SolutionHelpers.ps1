# Pure solution conversion helpers; CMake's originals remain in Build.
function Export-RootSolution {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$BuildRelative,
        [switch]$Development
    )
    $VisibleProjects = @('dxf_toolbox', 'dxf_foundation', 'dxf_support', 'dxf_runtime', 'dxf_gameplay', 'dxf_native_backends', 'Sandbox', 'Starter')
    if ([IO.Path]::GetExtension($Source) -eq '.slnx') {
        $Document = New-Object System.Xml.XmlDocument
        $Document.PreserveWhitespace = $true
        $Document.Load($Source)
        if (-not $Development) {
            foreach ($Project in @($Document.SelectNodes('//Project'))) {
                if ([IO.Path]::GetFileNameWithoutExtension($Project.GetAttribute('Path')) -notin $VisibleProjects) {
                    [void]$Project.ParentNode.RemoveChild($Project)
                }
            }
            foreach ($Dependency in @($Document.SelectNodes('//BuildDependency'))) {
                if ([IO.Path]::GetFileNameWithoutExtension($Dependency.GetAttribute('Project')) -notin $VisibleProjects) {
                    [void]$Dependency.ParentNode.RemoveChild($Dependency)
                }
            }
        }
        foreach ($Attribute in $Document.SelectNodes('//Project/@Path | //BuildDependency/@Project | //File/@Path')) {
            if (-not [IO.Path]::IsPathRooted($Attribute.Value)) {
                $Attribute.Value = $BuildRelative + $Attribute.Value.Replace('\', '/')
            }
        }
        $Document.Save($Destination)
        return
    }
    $Text = [IO.File]::ReadAllText($Source)
    $RemovedIds = @{}
    if (-not $Development) {
        # Remove complete project sections, including dependency sections owned by removed projects.
        $Text = [regex]::Replace($Text, '(?ms)^Project\("[^"\r\n]+"\) = "([^"\r\n]+)", "[^"\r\n]+", "({[^}\r\n]+})"\r?\n.*?^EndProject\r?\n', {
            param($Match)
            if ($Match.Groups[1].Value -notin $VisibleProjects) {
                $RemovedIds[$Match.Groups[2].Value] = $true
                return ''
            }
            return $Match.Value
        })
        # Configuration, nesting, and retained project dependencies must not reference removed GUIDs.
        $Text = [regex]::Replace($Text, '(?m)^.*{[0-9A-Fa-f-]+}.*\r?\n', {
            param($Match)
            foreach ($Id in $RemovedIds.Keys) {
                if ($Match.Value.IndexOf($Id, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                    return ''
                }
            }
            return $Match.Value
        })
    }
    $Text = [regex]::Replace($Text, '(?m)^(Project\("[^"\r\n]+"\) = "[^"\r\n]+", ")([^"\r\n]+)(",.*)$', {
        param($Match)
        $ProjectPath = $Match.Groups[2].Value
        if ($ProjectPath -match '\.(vcxproj|csproj|fsproj)$' -and -not [IO.Path]::IsPathRooted($ProjectPath)) {
            $ProjectPath = $BuildRelative.Replace('/', '\') + $ProjectPath
        }
        return $Match.Groups[1].Value + $ProjectPath + $Match.Groups[3].Value
    })
    [IO.File]::WriteAllText($Destination, $Text, (New-Object System.Text.UTF8Encoding($true)))
}
