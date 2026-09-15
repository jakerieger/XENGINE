param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [Parameter(Mandatory, ValueFromRemainingArguments)]
    [string[]]$Command
)

$OldPath = $env:PATH

try
{
    # Add Tools build dir to path so engine tools like PAKTool can simply be run as `PAKTool`
    # from the project root.
    $ToolsDir = Join-Path $PWD "build\$Configuration\bin\Tools"
    $env:PATH = "$ToolsDir;$env:PATH"

    $Exe = $Command[0]
    $Rest = @($Command | Select-Object -Skip 1)
    & $Exe @Rest
}
finally
{
    $env:PATH = $OldPath
}

exit $LASTEXITCODE