rng('shuffle')
numtests = 1;
numsamples = 10000;
lents = 0;

maxDepth = 1;
minelem = 5;
maxelem = 10;
maxElementsv = round(linspace(minelem,maxelem,numtests));
ignoreUnusables = true;
stride = numsamples;
mvgavgtime = zeros(stride,1);
mvgavgtimeload = zeros(stride,1);
avgnumelems = zeros(stride,1);
data = zeros(numtests,2);

doplot = false;
donames = true;
doCompare = false;
numelems = 0;
avgmultiplier = 0;

for j = 1:numtests
	maxElements = maxElementsv(j);
	for i = 1:numsamples
		if(mod(i,2) == 0)
			[ts1, avgnumelems(mod(i-1,stride)+1), names]  = randVarGen(maxDepth, maxElements, ignoreUnusables, donames, 'test_struct1');
			delete(['res/statetwofe' num2str(feature('getpid')) '.mat']);
            save(['res/stateonesh' num2str(feature('getpid')) '.mat']);
            mshshare(ts1);
            delete(['res/stateonesh' num2str(feature('getpid')) '.mat']);
            save(['res/stateonefe' num2str(feature('getpid')) '.mat']);
			x1 = mshfetch;

			if(doCompare)
			    [similarity,gmv,ld] = compstruct(x1, ts1);
			    if(~isempty(gmv) || ~isempty(ld))
				   %diffs = find(gmv ~= ld);
				   %mindiffind = min(find(gmv ~= ld));
				   save('res/failure.mat')
				   error('matshare failed 1');
			    end
			end

			timestr = sprintf('Test %d of %d | Sample %d of %d',j,numtests,i,numsamples);
			fprintf([repmat('\b',1,lents) timestr]);
			lents = numel(timestr);
		else
			[ts2, avgnumelems(mod(i-1,stride)+1), names]  = randVarGen(maxDepth, maxElements, ignoreUnusables, donames, 'test_struct1');
			delete(['res/stateonefe' num2str(feature('getpid')) '.mat']);
            save(['res/statetwosh' num2str(feature('getpid')) '.mat']);
            mshshare(ts2);
            delete(['res/statetwosh' num2str(feature('getpid')) '.mat']);
            save(['res/statetwofe' num2str(feature('getpid')) '.mat']);
			x2 = mshfetch;

			if(doCompare)
			    [similarity,gmv,ld] = compstruct(x2, ts2);
			    if(~isempty(gmv) || ~isempty(ld))
				   %diffs = find(gmv ~= ld);
				   %mindiffind = min(find(gmv ~= ld));
				   save('res/failure.mat')
				   error('matshare failed 2');
			    end
			end

			timestr = sprintf('Test %d of %d | Sample %d of %d',j,numtests,i,numsamples);
			fprintf([repmat('\b',1,lents) timestr]);
			lents = numel(timestr);
		end
			
	end
end