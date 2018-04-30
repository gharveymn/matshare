clear x

nums = 500000;
lents = 0;
disp(getmem);
times = zeros(nums,1);

tic
for i = 1:nums
	
	tic
	mshshare(i);
	x = mshfetch;
	mshclear;
	times(i) = toc;
	
% 	disp(a.shared.data);
% 	timestr = sprintf('%d/%d', i, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
toc
mshdetach
disp(getmem);