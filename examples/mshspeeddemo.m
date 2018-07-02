addpath(fullfile(fileparts(which(mfilename)),'..','scripts'));

mshpoolstartup;

% Calculate the sum of the matrix

n = 10000;
hn = n/2;
nsubmats = 4;

randmat = rand(n);

tic
nomres = sum(randmat(:));
toc

tic
submats = cell(1,nsubmats);
submats{1} = randmat(1:hn, 1:hn);
submats{2} = randmat(1:hn, (hn+1):end);
submats{3} = randmat((hn+1):end, 1:hn);
submats{4} = randmat((hn+1):end, (hn+1):end);

resparts = cell(1,nsubmats);
parfor i = 1:nsubmats
	resparts{i} = sum(submats{i}(:));
end
cnvres = sum([resparts{:}]);
toc

tic
mshpersistshare(randmat(1:hn,1:hn));
mshpersistshare(randmat(1:hn,(hn+1):end));
mshpersistshare(randmat((hn+1):end,1:hn));
mshpersistshare(randmat((hn+1):end,(hn+1):end));

parfor i = 1:nsubmats
	[~,newvars] = mshfetch;
	mshpersistshare(sum(newvars{i}(:)));
end
[~,mshresparts] = mshfetch;
mshres = sum([mshresparts{:}]);
toc

mshclear;