#!/bin/bash
VER=v2.1
OUT=fxtk-$VER
rm -rf $OUT fxtk-$VER.tar.gz
mkdir -p $OUT/demo-main $OUT/examples $OUT/docs
cp -r ../components $OUT/
cp *.c *.h *.sh $OUT/demo-main/ 2>/dev/null
rm -f $OUT/demo-main/fxtk_sim $OUT/demo-main/*.o
EXDIR=examples; [ -d "$EXDIR" ] || EXDIR=../examples
cp "$EXDIR"/*.c $OUT/examples/ && echo "✅ examples 拷入: $(ls $OUT/examples | wc -l) 个"
cp ../README.md $OUT/ 2>/dev/null
cp ../graph.png $OUT/ 2>/dev/null
cp ../texting.png $OUT/ 2>/dev/null
cp ../image.png $OUT/ 2>/dev/null
cp ../rending.png $OUT/ 2>/dev/null
cp ../docs/*.md $OUT/docs/ 2>/dev/null
find $OUT \( -name "*.bak" -o -name "*.orig" -o -name "*.rej" -o -name "*~" -o -name "*.o" \) -exec rm -rf {} + 2>/dev/null
tar czf fxtk-$VER.tar.gz $OUT
echo "🎉 重新打包完成, 自检:"
echo "   esp_log.h 在包内: $(tar tzf fxtk-$VER.tar.gz | grep -c 'esp_log.h')  (应>=1)"
echo "   examples 条目: $(tar tzf fxtk-$VER.tar.gz | grep -c 'examples/')  (应=19)"
