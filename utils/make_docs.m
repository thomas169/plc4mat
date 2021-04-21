function make_docs(recipe)

readmeSrc = 'readme.adoc';
adocSrc = {readmeSrc};
docsDir = 'docs';
projectRoot = fullfile(strrep(mfilename('fullpath'),mfilename,''),'..');
outDir = fullfile(projectRoot, docsDir);

for n = 1 : numel(adocSrc)
    file = fullfile(projectRoot, docsDir, adocSrc{n});
    makeCmd = sprintf('asciidoctor %s -D %s', file, outDir);
    system(makeCmd);
end

if (nargin >= 1) && (strcmp(recipe, 'push'))
    file = fullfile(projectRoot, docsDir, readmeSrc);
    copyfile(file, projectRoot);
end

end
