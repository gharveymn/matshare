clear x

nums = 5000000;
% memstride = 100;
% lents = 0;
% disp(getmem);
% times = zeros(nums,1);
% mems = zeros(nums/memstride,1);

tic
for i = 1:nums
	
	mshshare(i);
	x = mshfetch;
% 	if(mod(i,memstride) == 0)
% 		mems(i/memstride) = getmem;
% 	end
% 	disp(a.shared.data);
%  	timestr = sprintf('%d/%d', i, nums);
%  	fprintf([repmat('\b',1,lents) timestr]);
%  	lents = numel(timestr);
end
toc
% disp(getmem);
% plot(mems)