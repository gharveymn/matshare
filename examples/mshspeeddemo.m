addpath(fullfile(fileparts(which(mfilename)),'..','scripts'));

mshpoolstartup;

% Suppose you have four different data sets

n = 5000;
nmats = 4;
datasets = cell(1, nmats);

% which you want to sum
nomdatasums = cell(1, nmats);
cnvdatasums = cell(1, nmats);

for i = 1:nmats
	datasets{i} = rand(n);
end

tic
for i = 1:nmats
	nomdatasums{i} = sum(datasets{i});
end
toc

tic
parfor i = 1:nsubmats
	cnvdatasums{i} = sum(datasets{i}(:));
end
toc

tic
for i = 1:nmats
	mshpersistshare(datasets{i});
end

parfor i = 1:nsubmats
	[~,newvars] = mshfetch;
	mshpersistshare(sum(newvars{i}(:)));
end
[~,mshdatasums] = mshfetch;
toc

mshclear;