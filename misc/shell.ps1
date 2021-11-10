Import-Module posh-git
Import-Module oh-my-posh
Set-PoshPrompt -Theme powerlevel10k_modern

Write-Host Setting up build environment

$tempFile = [IO.Path]::GetTempFileName()  

Push-Location -Path .
cmd.exe /c " `"c:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat`" x64 && set > `"$tempFile`" " 

## Go through the environment variables in the temp file.  
## For each of them, set the variable in our local environment.  
Get-Content $tempFile | Foreach-Object {   
    if($_ -match "^(.*?)=(.*)$")  
    { 
        Set-Content "env:\$($matches[1])" $matches[2]  
    } 
}  

Remove-Item $tempFile

Pop-Location

# Add the misc folder to our path for easier execution of utilities
$env:Path = $PSScriptRoot + ";" + "e:\tools\4coder;" + $env:Path