rng('shuffle')
numtests = 1;
numsamples = 10000;
lents = 0;

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
dotimes = false;
numelems = 0;
avgmultiplier = 0;

savepath = fullfile(fileparts(mfilename('fullpath')), '..', 'res','states',currtime);
if(dosavestate)
	mkdirifnotexist(savepath)
end

for j = 1:numtests
	maxElements = maxElementsv(j);
	for i = 1:numsamples
		if(mod(i,2) == 1)
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
			if(dokeepall)
				eval(['mshshare(ts' num2str(i) '1);']);
			else
				mshshare(ts1);
			end
			
			x1 = mshfetch;
			
			if(dosavestate)
				if(i ~= 1)
					delete(fullfile(savepath, 'stateonefe.mat'));
				end
				save(fullfile(savepath, 'stateonefe.mat'));
			end
			
			if(docompare)
				[similarity,gmv,ld] = compstruct(x1, ts1);
				if(~isempty(gmv) || ~isempty(ld))
					%diffs = find(gmv ~= ld);
					%mindiffind = min(find(gmv ~= ld));
					save('res/compfailure.mat')
					error('matshare failed 1');
				end
			end
			
		else
			if(dokeepall)
				eval(['[ts' num2str(i) '2] = randVarGen(maxDepth, maxElements, ignoreUnusables);']);
			else
				[ts2] = randVarGen(maxDepth, maxElements, ignoreUnusables);
			end
			if(dosavestate)
				if(i ~= 2)
					delete(fullfile(savepath, 'statetwosh.mat'));
				end
				save(fullfile(savepath, 'statetwosh.mat'));
			end
			if(dokeepall)
				eval(['mshshare(ts' num2str(i) '2);']);
			else
				mshshare(ts2);
			end
			
			x2 = mshfetch;
			
			if(dosavestate)
				if(i ~= 2)
					delete(fullfile(savepath, 'statetwofe.mat'));
				end
				save(fullfile(savepath, 'statetwofe.mat'));
			end
			
			if(docompare)
				[similarity,gmv,ld] = compstruct(x2, ts2);
				if(~isempty(gmv) || ~isempty(ld))
					%diffs = find(gmv ~= ld);
					%mindiffind = min(find(gmv ~= ld));
					save('res/states/compfailure.mat')
					error('matshare failed 2');
				end
			end
		end
		
		if(mod(i,10)==0)
			timestr = sprintf('Test %d of %d | Sample %d of %d',j,numtests,i,numsamples);
			fprintf([repmat('\b',1,lents) timestr]);
			lents = numel(timestr);
		end
		
	end
end


fprintf('\n');