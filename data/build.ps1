
# TODO(james): Replace this script with an actual asset compiler

# Don't continue if any errors occur

# TODO(james): support other shader stages as well
# Grab all the files in the current folder ending with 'vert' or 'frag' and iterate over them
# one at a time, invoking the shader compiler for each
Get-ChildItem -Name -Include *glsl.vert,*glsl.frag | Foreach-Object {
    $shader = $_
    
    $filenameParts = $_.Split(".")
    $output = $filenameParts[0] + "." + $filenameParts[2] + ".spv"
    
    $shouldRebuild = $false

    if(Test-Path -Path $output)
    {
        if($(Get-Item $shader).LastWriteTime -gt $(Get-Item $output).LastWriteTime)
        {
            $shouldRebuild = $true
        }
    }
    else
    {
        $shouldRebuild = $true
    }

    if($shouldRebuild)
    {
        Write-Host "Compiling modified shader file $_ ..."
        glslc.exe $_ -o $output

        # Verify that it was successfull
        if($LASTEXITCODE -eq 0)
        {
            Write-Host "Successfully compiled $output ..."
        }
        else
        {
            Write-Host "Error! $_ failed to validate"
            Exit-PSSession
        }
    }
}