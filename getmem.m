function [mem] = getmem
	if(ispc)
		userview = memory;
		mem = userview.MemUsedMATLAB;
	else
		[~,out]=system('vmstat -s | grep "used memory"');
		mem=sscanf(out,'%f K used memory')*1024;
	end
end