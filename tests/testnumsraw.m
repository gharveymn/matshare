clear x

nums = 5000000;
lents = 0;
disp(getmem);
times = zeros(nums,1);

tic
for i = 1:nums
	
	% place in shm
	% set callback
	matshare_(uint8(0), i);
	
	% assign to x
	x = matshare_(uint8(1));
	
% 	disp(a.shared.data);
% 	timestr = sprintf('%d/%d', i, nums);
% 	fprintf([repmat('\b',1,lents) timestr]);
% 	lents = numel(timestr);
end
toc
disp(getmem);