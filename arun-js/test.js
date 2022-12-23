// import level from 'level' 
import { BaseTrie as Trie } from 'merkle-patricia-tree'
import fs from "fs";
import assert from 'node:assert/strict';

// load data
var data = JSON.parse(fs.readFileSync('./data1.json', 'utf8'));

const trie = new Trie()


async function insert() {
      // insert into trie
    for (const short of ["a", "b", "c", "d", "e", "f", "g", "h"]) {
        let address = "testKey" + short// + "y".repeat(3)
        console.log("[state-change item]", short, address)
        console.log("address in bytes", Buffer.from(address))
        await trie.put(Buffer.from(address), Buffer.from("testVal" + short))
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
  for (const short of ["a", "b", "c", "d", "e", "f", "g", "h"]) {
    let address = "x".repeat(3) + short + "y".repeat(3)
        // await trie.put(Buffer.from(address), Buffer.from("1234"))
        let proof = await Trie.createProof(trie, Buffer.from(address))
        let proofValue = await Trie.verifyProof(trie.root, Buffer.from(address), proof)
        console.log("[proof]", short, address, proof.length, proofValue.toString())
        // assert.deepStrictEqual(fetchValue, proofValue)
    }
    //   let fetchValue = await trie.get(Buffer.from(address))
}

console.time('test');
await trie.put(Buffer.from("testKey"), Buffer.from("hello"))
await insert()
// await insertAndFetch()
// await fetchAndProve()
const node1 = await trie.findPath(Buffer.from('testKey')) // We attempt to retrieve the node using the key "testKey"
console.log('Node 1: ', node1.node) // null


const node2 = await trie.findPath(Buffer.from('testKeya')) // We attempt to retrieve the node using the key "testKey"
console.log('Node 2: ', node2.node) 


// console.log("Trie now at ", JSON.stringify(trie).length);
// console.log(Buffer.from("123abcdXYZW"))
console.timeEnd('test');
