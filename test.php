<?php
$a = base32_encode("中华人民共和国");
echo "密文：$a\n";
echo "明文：". base32_decode($a). "\n";
