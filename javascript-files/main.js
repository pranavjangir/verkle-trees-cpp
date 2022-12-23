// import level from 'level' 
import { BaseTrie as Trie } from 'merkle-patricia-tree'
import fs from "fs";
import assert from 'node:assert/strict';

// load data
var data = JSON.parse(fs.readFileSync('./data1.json', 'utf8'));

// const db = new level('./testdb')
// const trie = new Trie(db)
const trie = new Trie()

// async function test() {
//   await trie.put(Buffer.from('test'), Buffer.from('one'))
//   const value = await trie.get(Buffer.from('test')
//   console.log(value.toString()) // 'one'
// }


// async function test2() {
  //   await trie.put(Buffer.from('test'), Buffer.from('one'))
  //   await trie.put(Buffer.from('test1'), Buffer.from('two'))
  //   await trie.put(Buffer.from('test2'), Buffer.from('three'))
  //   const proof = await Trie.createProof(trie, Buffer.from('test'))
  //   const value = await Trie.verifyProof(trie.root, Buffer.from('test'), proof)
  //   console.log("proof is ", proof)
  //   console.log(value.toString()) // 'one'
  //   // console.log(JSON.stringify(trie))
  //   const roughObjSize = JSON.stringify(trie).length;
  //   console.log(roughObjSize)
  // }
  
// test()
// test2()

function insert() {
  for (var blockNumber in data) {
    // if (blockNumber == "f6edc6") {
      // break
    // }
    for (var idx in data[blockNumber]) {
      // if (idx == 20) {
        // break
      // }
      var address = data[blockNumber][idx]
      var randValue = parseInt(Math.random() * 1000).toString();
      // console.log("[state-change item]", blockNumber, idx, address, randValue)

      // insert into trie
      trie.put(Buffer.from(address), Buffer.from(randValue))
    }
  }

}

async function insertAndFetch() {

    for (var blockNumber in data) {
      if (blockNumber == "f6edc6") {
        break
      }
      for (var idx in data[blockNumber]) {
        if (idx == 20) {
          break
        }
        var address = data[blockNumber][idx]
        var randValue = parseInt(Math.random() * 1000).toString();
        console.log("[state-change item]", blockNumber, idx, address, randValue)

        // insert into trie
        await trie.put(Buffer.from(address), Buffer.from(randValue))

        // fetch from trie
        var fetchValue = await trie.get(Buffer.from(address))
        
        console.log("[fetch & verify]", blockNumber, idx, address, fetchValue, randValue)
        assert.deepStrictEqual(fetchValue.toString(), randValue)

        // // get proof and verify
        // var proof = await Trie.createProof(trie, Buffer.from(address))
        // var proofValue = await Trie.verifyProof(trie.root, Buffer.from(address), proof)
        // console.log("proof", address, proofValue.toString(), randValue)

      }
    }
    // const roughObjSize = JSON.stringify(trie).length;
}

async function fetchAndProve() {
  for (var blockNumber in data) {
    // if (blockNumber == "f6edc6") {
    //   break
    // }
    for (var idx in data[blockNumber]) {
      // if (idx == 20) {
      //   break
      // }
      var address = data[blockNumber][idx]
      let fetchValue = await trie.get(Buffer.from(address))

      let proof = await Trie.createProof(trie, Buffer.from(address))
      let proofValue = await Trie.verifyProof(trie.root, Buffer.from(address), proof)
      console.log("[proof]", blockNumber, idx, address, proof.length, fetchValue.toString(), proofValue.toString())
      assert.deepStrictEqual(fetchValue, proofValue)
    }
  }
}

console.time('test');
await insert()
// await insertAndFetch()
// await fetchAndProve()
// console.log("Trie now at ", JSON.stringify(trie).length);
// console.log(Buffer.from("123abcdXYZW"))
console.timeEnd('test');
