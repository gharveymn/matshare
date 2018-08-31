function tv = testunaryvarop(mshf, matf, tv)
	x = matshare.share(tv);
	if(mshf(x) ~= matf(tv))
		error('incorrect result');
	end
end
