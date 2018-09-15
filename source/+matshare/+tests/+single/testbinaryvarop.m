function tv = testbinaryvarop(mshf, matf, tv, tv2)
	x = matshare.share(tv);
	if(mshf(x, tv2) ~= matf(tv,tv2))
		error('incorrect result');
	end
end