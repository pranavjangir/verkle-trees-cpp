// const { Trie } = require("@ethereumjs/trie"); // We import the library required to create a basic Merkle Patricia Tree
import { BaseTrie as Trie } from 'merkle-patricia-tree'
// import pkg from 'keccak256';
// const keccak256 = require('keccak256')
import keccak256 from 'keccak256'

const trie = new Trie();

await trie.put(Buffer.from('testKey'), Buffer.from('testValue'))

await trie.put(Buffer.from('testKey00001'), Buffer.from('testValuea'))
await trie.put(Buffer.from('testKey00002'), Buffer.from('testValueb'))
await trie.put(Buffer.from('testKey00003'), Buffer.from('testValuec'))
await trie.put(Buffer.from('testKey00004'), Buffer.from('testValued'))
await trie.put(Buffer.from('testKey00005'), Buffer.from('testValuee'))
await trie.put(Buffer.from('testKey0000X'), Buffer.from('testValuex'))

// const node1 = await trie.findPath(Buffer.from('testKey')) // We retrieve the node at the "branching" off of the keys
// console.log('Node: ', node1.node) // A branch node!
// console.log('Node value: ', node1.node._value.toString()) // The branch node's value, in this case an empty buffer since the key "testValue" isn't assigned a value

// const node3 = await trie.lookupNode(Buffer.from(node1.node._branches[3])) // Looking up the child node from its hash (using lookupNode)
// console.log(node3) // An extension node!

// const node4 = await trie.lookupNode(Buffer.from(node3._value))
//   console.log(node4)

// const node2 = await trie.findPath(Buffer.from('testKey0001')) // We retrieve the node at the "branching" off of the keys
// console.log('Node2: ', node2.node) // A branch node!
// console.log('Node value: ', node2.node._value.toString()) // The branch node's value, in this case an empty buffer since the key "testValue" isn't assigned a value



// console.log(node1.node._branches[3].toString())

const node1 = await trie.findPath(Buffer.from('testKey'))
console.log(node1.node)
// console.log('Extension node hash: ', node1.node._branches[3])

const node2 = await trie.lookupNode(Buffer.from(node1.node._branches[3]))
console.log('Value of extension node: ', node2)

const node3 = await trie.lookupNode(Buffer.from(node2._value))
console.log('Value of extension node: ', node3)

const node4 = await trie.lookupNode(node3._branches[3])
console.log('Value of extension node: ', node4)

const node5 = await trie.lookupNode(node4._branches[2])
console.log('Value of extension node: ', node5._value.toString())

let proof = await Trie.createProof(trie, Buffer.from("testKey00002"))
console.log("proof is", proof)
let out = proof.map(node => ({
    "key": keccak256(node),
    // "xter": trie.findPath(keccak256(node)),
    "value": node.toString(),
  }))


console.log(out)
// out.forEach(x => (console.log(x["xter"].then(n => console.log(n)))))
// proof.forEach(element => {
//     // console.log(proof.toString())
//     const nodex = trie.findPath(element)
//     // console.log(nodex)
//     nodex.then(x => console.log(x))
// });
console.log("-------")

// async function tester(trie, key) {
//     var stack = await trie.findPath(key)
//     // stack.then(console.log)
//     console.log(stack.serialize())
//     const p = stack.map((stackElem) => {
//       return stackElem.serialize()
//     })
//     return p
//   }


// //   tester(trie, Buffer.from("testKey00002"))
// var stack = await trie.findPath(Buffer.from("testKey00002"))
// // console.log(stack.map(console.log))
// console.log(stack)
// // const p = await trie.findPath(key)
// //     const p = stack.map((stackElem) => {
// //       return stackElem.serialize()
// //     })