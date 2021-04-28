function make_docs(recipe)

readmeSrc = 'readme.adoc';
adocSrc = {readmeSrc};
docsDir = 'docs';
imgDir = 'resources';
projectRoot = regexprep(fullfile(strrep(mfilename('fullpath'),mfilename,'')),'[^/]+/$','');
outDir = fullfile(projectRoot, docsDir);
imgsDir =  fullfile(projectRoot, docsDir, imgDir);

resDir = '..';

for n = 1 : numel(adocSrc)
    file = fullfile(projectRoot, docsDir, adocSrc{n});
    makeCmd = sprintf('asciidoctor %s -D %s -a imagesdir=%s', file, outDir, resDir);
    system(makeCmd);
end

if (nargin >= 1) && (strcmp(recipe, 'push'))
    file = fullfile(projectRoot, docsDir, readmeSrc);
    copyfile(file, projectRoot);
end

end
