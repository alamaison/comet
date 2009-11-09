:: %1 - filename
sed -e "s/tlb2h/comet/g" -e "s/TLB2H/COMET/g" -e "s/Tlb2h/Comet/g" <%1 >_%1
del %1
rename _%1 %1
