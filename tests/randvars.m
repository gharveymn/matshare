rng('shuffle')
numtests = 10;
numsamples = 100000;
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
doCompare = true;
numelems = 0;
avgmultiplier = 0;

a = MatShare;

for j = 1:numtests
	maxElements = maxElementsv(j);
	for i = 1:numsamples
		[test_struct1, avgnumelems(mod(i-1,stride)+1), names]  = randVarGen(maxDepth, maxElements, ignoreUnusables, donames, 'test_struct1');
		a.data = test_struct1;
		x = a.data;
		
		timestr = sprintf('Test %d of %d | Sample %d of %d',j,numtests,i,numsamples);
		fprintf([repmat('\b',1,lents) timestr]);
		lents = numel(timestr);
	end
end