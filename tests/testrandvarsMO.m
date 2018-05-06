rng('shuffle')
numtests = 1;
numsamples = 100000;
lents = 0;

clearstridemax = 15;
clearstride = 5;

mshclearstridemax = 100;
mshclearstride = 50;

maxDepth = 2;
minelem = 0;
maxelem = 1000;
maxElementsv = round(linspace(minelem,maxelem,numtests));
ignoreUnusables = true;
stride = numsamples;
mvgavgtime = zeros(stride,1);
mvgavgtimeload = zeros(stride,1);
data = zeros(numtests,2);
pidstr = num2str(feature('getpid'));
currtime = num2str(now);

doplot = false;
docompare = false;
dokeepall = false;
dosavestate = false;
numelems = 0;
avgmultiplier = 0;

times = zeros(numsamples,1);

savepath = fullfile(fileparts(mfilename('fullpath')), '..', 'res','states',currtime);
if(dosavestate)
	mkdirifnotexist(savepath)
end

disp(getmem)

for j = 1:numtests
	maxElements = maxElementsv(j);
	for i = 1:numsamples
		
		if(dokeepall)
			eval(['[ts' num2str(i) '1] = randVarGen(maxDepth, maxElements, ignoreUnusables);']);
		else
			[ts1] = randVarGen(maxDepth, maxElements, ignoreUnusables);
		end
		if(dosavestate)
			if(i ~= 1)
				delete(fullfile(savepath, 'stateonesh.mat'));
			end
			save(fullfile(savepath, 'stateonesh.mat'));
		end
		
		tic;
		if(dokeepall)
			eval(['mshshare(ts' num2str(i) '1);']);
		else
			mshshare(ts1);
		end
		
		[x,y,z] = mshfetch;
		times(i) = toc;
		
		if(dosavestate)
			if(i ~= 1)
				delete(fullfile(savepath, 'stateonefe.mat'));
			end
			save(fullfile(savepath, 'stateonefe.mat'));
		end
		
		if(docompare)
			[similarity,gmv,ld] = compstruct(x, ts1);
			if(~isempty(gmv) || ~isempty(ld))
				%diffs = find(gmv ~= ld);
				%mindiffind = min(find(gmv ~= ld));
				save('res/compfailure.mat')
				error('matshare failed 1');
			end
		end
		
		if(mod(i,10)==0)
			timestr = sprintf('Test %d of %d | Sample %d of %d',j,numtests,i,numsamples);
			fprintf([repmat('\b',1,lents) timestr]);
			lents = numel(timestr);
		end
		
		if(mod(i,clearstride) == 0)
			clear x y z
			clearstride = randi(clearstridemax);
		end
		
		if(mod(i,mshclearstride) == 0)
			mshclear
			mshclearstide = randi(mshclearstridemax);
		end
		
		
	end
	
	plot(movmean(times,round(numel(times)/10)));
	
end

disp(getmem)


fprintf('\n');